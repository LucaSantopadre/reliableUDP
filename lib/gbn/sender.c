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



void sendtoGBN(int socket, struct sockaddr_in *receiver_addr, int N, int lost_prob, int fd) {

	char *buff = calloc(PKT_SIZE, sizeof(char));
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
	logger("INFO", __func__,__LINE__, "Thread for ack created");

	srand(time(NULL));
	initParams(N);
	pkts=calloc(N, sizeof(packet));
	status_pkts=calloc(N, sizeof(int));
	memset(status_pkts, 0, N*sizeof(int));


	// CALCOLO TOTALE PACCHETTI
	file_dim = lseek(fd, 0, SEEK_END); // in byte
	if(file_dim%PAYLOAD==0){
		tot_pkts = file_dim/PAYLOAD;
	}
	else{
		tot_pkts = file_dim/PAYLOAD+1;
	}
	lseek(fd, 0, SEEK_SET);
	

	// INIT SEQ_NUM PKTS AND WINDOW
	win_end = MIN(tot_pkts, sendBase + WINDOW - 1);
	for(i=sendBase; i<win_end+1; i++){
		//printf("----- pkt %d\n",i);
		pkts[i].seq_num = i;
		pkts[i].pkt_dim=read(fd, pkts[i].data, PAYLOAD);
		if(pkts[i].pkt_dim==-1){
			pkts[i].pkt_dim=0;
		}
	}

	//INT TX
	gettimeofday(&startTime, NULL);
	
	getStatusParam("INIT");

	while(tot_acked<tot_pkts && err_count<MAX_ERR){ //while ho pachetti da inviare e non ho MAX_ERR ricezioni consecutive fallite
		sendWindow();
	}

	getStatusParam("END");
	
	
	//END TX -> SEND "-1" PKT
	for(i=0; i<MAX_ERR; i++){
		memset(buff, 0, PKT_SIZE);
		((packet*)buff)->seq_num=-1;
		if(sendto(sock, buff, sizeof(int), 0, (struct sockaddr *)rcv_addr, addr_len) > 0) {
			printf("File transfer finished\n");
			gettimeofday(&endTime, NULL);
			double tm=endTime.tv_sec-startTime.tv_sec+(double)(endTime.tv_usec-startTime.tv_usec)/1000000;
			double tp=file_dim/tm;
			printf("Transfer time: %f sec [%f KB/s]\n", tm, tp/1024);
			close(fd);
			return;
		}
	}

	printf("File transfer failed\n");
	close(fd);
	return;
}


void sendWindow(){
	int i;

	signal(SIGALRM, timeoutRoutine); 

	for(i=next_seq_num; i<win_end+1; i++){
		//printf("ppp %d\n",i);
		if (!isTimerStarted){
			startTimer(timeoutInterval); // setta timer per il piu vecchio pktto non acked
			isTimerStarted = true;
		}
		sendPkt(i);
		next_seq_num++;
	}
}


void sendPkt(int i){
	logger("INFO", __func__,__LINE__, "--------Send pkt");
	getStatusParam("Send pkt");
	if (sendto(sock, pkts+i, PKT_SIZE, 0, (struct sockaddr *)rcv_addr, addr_len)<0){
		perror ("Sendto Error");
		exit(-1);
	}
	if (status_pkts[i] == 0){
		status_pkts[i] = 1;
	}
}


void initParams(int N){
	sendBase=1;
	next_seq_num = 1;
	window=N;
	max=window-1;
	
	tot_acked=0;
	tot_sent=0;
	err_count=0;
	isTimerStarted = 0;
	timeoutInterval = TIMEOUT_PKT;

}


void getStatusParam(char *step){
	printf("\n------------- %s -----------\n",step);
	printf("filedim  %lu\n",file_dim);
	printf("PAYLOAD  %lu\n",PAYLOAD);
	printf("tot_pkts %d\n",tot_pkts);
	printf("N        %d\n",window);
	printf("sendBase %d ----- %d win_end\n",sendBase,win_end);
	printf("next_seq_n  %d\n",next_seq_num);
	printf("------------------------\n");

}

void *recvACK(void *arg){
	struct thread_args *args = arg;
	int socket = args->sock;
	struct sockaddr_in *rcv_addr = args->rcv_addr;
	socklen_t addr_len = args->addr_len;
	
	int duplicate_ack_count = 1;
	int old_acked;
	int ack_num = 0;

	// ACK cases
	while(tot_acked<=tot_pkts){
		memset(&pkt_rcv, 0, sizeof(packet));

		if (recvfrom(sock, &ack_num, sizeof(int), 0, (struct sockaddr *)rcv_addr, &addr_len) < 0){
			logger("ERROR",__func__,__LINE__, "Error receiving ack");
			exit(-1);
		}
		// ACK ok
		if(ack_num >= sendBase){
			logger("INFO",__func__,__LINE__, "Ack in window");
			sendBase = ack_num;
			win_end = MIN(tot_pkts, sendBase + WINDOW -1);
			duplicate_ack_count = 1;
			tot_acked++;
			for (int k = sendBase-1; k<ack_num-1; k++){
				status_pkts[k] = 2;
			}
			if (tot_acked == tot_pkts){
				startTimer(0);
				logger("INFO", __func__,__LINE__, "Thread for ack end");
				break;
			}
		}
		// ACK duplicated
		else { 
			logger("INFO",__func__,__LINE__, "Ack duplicated");
			duplicate_ack_count++;
		}
	}
}



// timeout routine alarm
void timeoutRoutine(){
	printf("\n\nTIMEOUTEXPIRED\n\n");
	isTimerStarted = false;
	//retransmission(sendBase-1, "TIMEOUT EXPIRED");
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

