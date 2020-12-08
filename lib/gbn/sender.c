#include "../basic.h"


int err_count;//conta quante volte consecutivamente è fallita la ricezione
off_t file_dim;
int tot_pkts, tot_acked, tot_sent;
int sendBase, max, window, next_seq_num, win_end;
bool isTimerStarted;
int timeoutInterval;

packet *pkts, pkt_rcv;
int *status_pkts; // 0 = Da Inviare // 1 = Inviato, non Acked // 2 = Acked

int sock;
socklen_t addr_len;
struct sockaddr_in *rcv_addr;


// Struttura per passare argomenti al thread per la ricezione degli ACK
pthread_t thread;
struct thread_args
 {
	int sock;
    struct sockaddr_in *rcv_addr;
    socklen_t addr_len;
};

void initParams(int N);
void getStatusParam(char *step);
void *recvACK(void *arg);
void sendPkt(int i);
void timeoutRoutine();
void startTimer(int micro);
void sendWindow();
void increase_timeout();
void decrease_timeout();

void initParams(int N){
	sendBase=1;
	next_seq_num = 1;
	window=N;
	max=window-1;
	
	tot_acked=0;
	tot_sent=0;
	err_count=0;
	isTimerStarted = false;
	timeoutInterval = TIMEOUT_PKT;

}


void getStatusParam(char *step){
	printf("------------- %s -----------\n",step);
	printf("next_seq_n %d\n",next_seq_num);
	printf("sendBase   [%d ----- %d] win_end\n",sendBase,win_end);
	printf("N          %d\n",window);
	printf("filedim    %lu\n",file_dim);
	printf("PAYLOAD    %lu\n",PAYLOAD);
	printf("tot_pkts   %d\n",tot_pkts);
	printf("------------------------\n\n");

}


int sendtoGBN(int socket, struct sockaddr_in *receiver_addr, int N, int lost_prob, int fd) {

	
	int i;
	struct timeval endTime, startTime;
	
	// INIT SOCKET
	sock = socket;
	rcv_addr = receiver_addr;
	addr_len = sizeof(struct sockaddr_in);

	// INIT ACK'S THREAD
	struct thread_args t_args;
	t_args.sock = sock;
	t_args.rcv_addr = rcv_addr;
	t_args.addr_len = addr_len;
	int ret = pthread_create(&thread,NULL,recvACK,(void*)&t_args);
	//logger("INFO", __func__,__LINE__, "Thread for ack created\n");

	// CALCOLO TOTALE PACCHETTI
	file_dim = lseek(fd, 0, SEEK_END); // in byte
	if(file_dim%PAYLOAD==0){
		tot_pkts = file_dim/PAYLOAD;
	}
	else{
		tot_pkts = file_dim/PAYLOAD+1;
	}
	lseek(fd, 0, SEEK_SET);

	// INIT params and buffers
	srand(time(NULL));
	initParams(N);
	char *buff = calloc(PKT_SIZE, sizeof(char) + 8);
	pkts=calloc(tot_pkts + 1, sizeof(packet) );
	memset(pkts, 0, (tot_pkts + 1)*sizeof(packet) );

	// INIT SEQ_NUM PKTS AND WINDOW
	win_end = MIN(tot_pkts, sendBase + WINDOW - 1);
	for(i=sendBase; i<tot_pkts+1; i++){
		pkts[i].seq_num = i;
		pkts[i].pkt_dim=read(fd, pkts[i].data, PAYLOAD);
		if(pkts[i].pkt_dim==-1){
			pkts[i].pkt_dim=0;
		}
	}

	//INT TX
	gettimeofday(&startTime, NULL);
	
	//getStatusParam("**************INIT");

	while(tot_acked<tot_pkts && err_count<MAX_ERR){ //while ho pachetti da inviare e non ho MAX_ERR ricezioni consecutive fallite
		sendWindow();
	}

	//getStatusParam("**************END");
	if(err_count==MAX_ERR){
		printf("File transfer failed (inactive receiver)\n");
		return -1;
	}
	
	//END TX -> SEND "-1" PKT
	memset(buff, 0, PKT_SIZE);
	((packet*)buff)->seq_num=-1;
	if(sendto(sock, buff, sizeof(int), 0, (struct sockaddr *)rcv_addr, addr_len) > 0) {
		startTimer(0);
		alarm(0);
		logger("INFO", __func__,__LINE__, "File transfered, time: ");
		gettimeofday(&endTime, NULL);
		double tm=endTime.tv_sec-startTime.tv_sec+(double)(endTime.tv_usec-startTime.tv_usec)/1000000;
		double tp=file_dim/tm;
		double kbps= tp/1024;

		char str[20];
		sprintf(str, "%lf,\n", kbps);

		int fd_res = open("results.csv", O_CREAT | O_APPEND | O_RDWR, 0666); 
		write(fd_res, str, strlen(str));
		close(fd_res);

		if(kbps < 1000.0){
			printf("%f sec [%f KB/s]\n", tm, kbps);
		}else {
			tp = tp/1024;
			printf("%f sec [%f MB/s]\n", tm, tp/1024);
		}
		
		
		return 0;
	}
	
	logger("ERROR", __func__,__LINE__, "File transfert failed\n");
	return -1;
}


void sendWindow(){
	int i;

	signal(SIGALRM, timeoutRoutine); 
	for(i=next_seq_num; i<win_end+1; i++){
		if (!isTimerStarted){
			//printf("timeout for pkt %d , us: %d\n",i, timeoutInterval);
			startTimer(timeoutInterval); // setta timer per il piu vecchio pktto non acked
			isTimerStarted = true;
		}
		sendPkt(i);
		next_seq_num++;
	}
}


void sendPkt(int i){
	if(!packet_lost(LOST_PROB)){
		//logger("INFO", __func__,__LINE__, "Send pkt: ");
		//printf("%d, ",i);
		//printf("window: [%d ----- %d]\n",sendBase,win_end);
		//getStatusParam("Send pkt");
		if (sendto(sock, pkts+i, PKT_SIZE, 0, (struct sockaddr *)rcv_addr, addr_len)<0){
			perror ("Sendto Error");
			exit(-1);
		}
	} else{
		//LOST
		//logger("WARN", __func__,__LINE__, "Lost pkt: ");
		//printf("%d\n",i);
	}
}


void *recvACK(void *arg){
	struct thread_args *args = arg;
	int socket_ack = args->sock;
	struct sockaddr_in *rcv_addr = args->rcv_addr;
	socklen_t addr_len = args->addr_len;
	
	int old_acked;
	int ack_num = 0;
	int expected_ack = 1;
	signal(SIGALRM, timeoutRoutine); 
	while(tot_acked<=tot_pkts){	
		if (recvfrom(socket_ack, &ack_num, sizeof(int), 0, (struct sockaddr *)rcv_addr, &addr_len) < 0){
			//logger("ERRO",__func__,__LINE__, "Error receiving ack\n");
			err_count++;
			if(ADAPTIVE) {
				//printf("INCerr\n");
				increase_timeout();
				isTimerStarted = false;
			}
		}else {
			//logger("INFO",__func__,__LINE__, "Ack received: ");
			//printf("%d\n",ack_num);
			// ACK cumulative
			if(ack_num >= sendBase && ack_num >= expected_ack && ack_num <= win_end){
				if(ADAPTIVE) {
					//printf("DECack\n");
					decrease_timeout();
				}
				sendBase = ack_num+1;
				win_end = MIN(tot_pkts, sendBase + WINDOW -1);
				tot_acked = ack_num;
				expected_ack = sendBase;
				startTimer(timeoutInterval);  // parte il timer per il prossimo pacchetto
				isTimerStarted = true;
				//printf("ACKtimeout for pkt %d , us: %d\n",expected_ack, timeoutInterval);
				if (tot_acked == tot_pkts){
					startTimer(0);
					//logger("INFO", __func__,__LINE__, "Thread for ack end\n");
					break;
				}
			}
			// ACK duplicated
			else if(ack_num < expected_ack){ 
				if(ADAPTIVE) {
					//printf("INCdup\n");
					increase_timeout();
				}
			}
		}
		
	}
}


// timeout routine alarm
void timeoutRoutine(){
	if(ADAPTIVE) {
		//printf("INCtimeout\n");
		increase_timeout();
	}
	//logger("WARN", __func__,__LINE__, "TIMEOUTEXPIRED\n");
	isTimerStarted = false;
	next_seq_num = sendBase;
	sendWindow();
	return;
}


// Imposta il timer di ritrasmissione
void startTimer(int t){
    struct itimerval it_val;
	if (t >= MAX_RTO){
		t = MAX_RTO;
	}
	it_val.it_value.tv_sec = 0;
	it_val.it_value.tv_usec = t;
	it_val.it_interval.tv_sec = 0;
	it_val.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
		perror("Set Timer Error:");
		exit(1);
	}
}


void decrease_timeout(){
	if(timeoutInterval >= MIN_TIMEOUT + TIME_UNIT){
		timeoutInterval = timeoutInterval - TIME_UNIT;
	}
}

void increase_timeout(){
	if(timeoutInterval <= MAX_TIMEOUT - TIME_UNIT){
		timeoutInterval = timeoutInterval + TIME_UNIT;
	}
}

