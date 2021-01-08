#include "../basic.h"

int error_count;
int max;
packet pkt_aux;
int expected_seq_num;
int sock;
socklen_t addr_len;
struct sockaddr_in *send_addr;


void recvPkt(int fd){
	int i, seq_num;
	bool toWrite = true;

	if((recvfrom(sock, &pkt_aux, PKT_SIZE, 0, (struct sockaddr *)send_addr, &addr_len) < 0) && (error_count<MAX_ERR)) {
		error_count++;
		return;
	}
	seq_num = pkt_aux.seq_num;
	// End pkt?
	if(seq_num==-1) {
		return;
	}
	
	// expected?
	if(seq_num == expected_seq_num){
		// YES
		error_count = 0;
		expected_seq_num++;
	} else{
		// NO
		seq_num = expected_seq_num - 1;
		toWrite = false;
	}
	//send ack
	if(sendto(sock, &seq_num, sizeof(int), 0, (struct sockaddr *)send_addr, addr_len) < 0) {
		logger("ERROR", __func__, __LINE__, "Error sending ack\n");
	}
	if(toWrite){
		write(fd, pkt_aux.data, pkt_aux.pkt_dim); 
	}
	memset(&pkt_aux, 0, sizeof(packet));
}



// receive file with GBN protocol
int recvfromGBN(int socket, struct sockaddr_in *sender_addr, int loss_prob, int fd){
	sock = socket;
	addr_len = sizeof(struct sockaddr_in);
	send_addr = sender_addr;
	error_count = 0;
	expected_seq_num = 1;
	srand(time(NULL));

	logger("INFO", __func__, __LINE__, "File transfer started...\n");
	while(pkt_aux.seq_num!=-1 && error_count<MAX_ERR){
		recvPkt(fd);
	}
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

