#include "../basic.h"

int error_count;
int base, max, window;
packet pkt_aux;
int expected_seq_num;

off_t file_dim;


int sock;
socklen_t addr_len;
struct sockaddr_in *client_addr;

bool toLost = true;



void recvPkt(int fd){
	int i, seq_num;
	bool toWrite = true;


	//memset(&pkt_aux, 0, sizeof(packet));
	if((recvfrom(sock, &pkt_aux, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len) < 0) && (error_count<MAX_ERR)) {
		error_count++;
		return;
	}
	seq_num = pkt_aux.seq_num;
	//se è il pacchetto di fine file, termino
	if(seq_num==-1) {
		return;
	}
	
	// expected?
	if(seq_num == expected_seq_num){
		// ok
		//printf("PKT RECEIVED %d\n",seq_num);
		//printf("DATA RECEIVED:\n%s\n",pkt_aux.data);
		error_count = 0;
		expected_seq_num++;
	} else{
		// not expected
		//printf("err received %d, expected %d\n",seq_num,expected_seq_num);
		seq_num = expected_seq_num - 1;
		//error_count++;
		toWrite = false;
	}

	//invio ack
	if(sendto(sock, &seq_num, sizeof(int), 0, (struct sockaddr *)client_addr, addr_len) < 0) {
		logger("ERROR", __func__, __LINE__, "Error sending ack\n");
	}
	//printf("ack inviato %d\n", seq_num);
	if(toWrite){
		write(fd, pkt_aux.data, pkt_aux.pkt_dim); 
	}
	memset(&pkt_aux, 0, sizeof(packet));
}




int recvfromGBN(int socket, struct sockaddr_in *sender_addr, int loss_prob, int fd){
	sock = socket;
	addr_len = sizeof(struct sockaddr_in);
	client_addr = sender_addr;
	error_count = 0;
	expected_seq_num = 1;
	
	srand(time(NULL));

	logger("INFO", __func__, __LINE__, "File transfer started...\n");

	while(pkt_aux.seq_num!=-1 && error_count<MAX_ERR){//while ho pachetti da ricevere
		recvPkt(fd);
	}
	//printf("error: %d\n",error_count);
	memset(&pkt_aux, 0, sizeof(packet));
	if(error_count==MAX_ERR){
		logger("ERROR", __func__, __LINE__, "File transfer error (inactive sender)...\n");
		return -1;
	}
	else{
		logger("INFO", __func__, __LINE__, "File transfer END\n");
		return 0;
	}
}

