#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include "./lib/common.h"
#include "./lib/utils.h"
#include "./lib/gbn/sender.h"
#include "./lib/gbn/receiver.h"

int createClientSocketAndBind(int *c_sock, struct sockaddr_in *s_addr, int port);
void handshakeClient (int, struct sockaddr_in*);
void closeConnectionClient (int client_sock, struct sockaddr_in *server_addr);
void alarmTimeout();



int main (int argc, char** argv) {
	int res, menu_choiche, bytes, num_files;
	int client_sock;
	int list = LIST, get = GET, put = PUT, close_conn = CLOSE;
	struct sockaddr_in server_address;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	char *path = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(server_address);
	int file_des;
	off_t end_file, file_control;
	char *list_files[MAX_FILE_LIST];
	char *list_files_client[MAX_FILE_LIST];
	char buf[1200];
conn:
	createClientSocketAndBind(&client_sock , &server_address, SERVER_PORT);

	handshakeClient(client_sock, &server_address);
  	memset(buff, 0, sizeof(buff));

	//READY
	res = recvfrom(client_sock, buff, strlen(READY), 0, (struct sockaddr *)&server_address, &addr_len);
	if (res < 0) {
		logger("ERROR", __func__, __LINE__, "Server dispaching error\n");
		exit(-1);
	}
	printf(" WELCOME, you are connected to the server!\n");

init:
	num_files = getFiles(list_files_client, CLIENT_DIR);
	if(num_files == 0){
		printf(" Your dir is empty!\n");
	} else {
		int i=0;
		printf("\n--------------------- LOCAL FILE LIST ---------------------\n");
		while(i<num_files) {
			printf("%s\n", list_files_client[i]);
			i++;
		}
	}
	printf("-----------------------------------------------------------\n");
	printf("============== COMMAND LIST ================\n");
	printf("1) List available files on the server\n");
	printf("2) Download a file from the server\n");
	printf("3) Upload a file to the server\n");
	printf("4) Close connection\n");
	printf("============================================\n");
	printf("> Choose an operation (%d seconds): ", REQUEST_SEC);
	signal(SIGALRM, alarmTimeout); 
	alarm(REQUEST_SEC);
	if(scanf ("%d",&menu_choiche) > 0) {
		alarm(0);
	}

	switch (menu_choiche)
	{
	case LIST:
		res = sendto(client_sock, (void*)&list, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len);
		if(res < 0){
			logger("ERROR", __func__, __LINE__, "Request LIST failed (sendto)\n");
			exit(-1);
		}
		
		int fd = open("files/client/file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666); //Apro il file con la lista dei file del server
			
		// Inizio la ricezione del file
		if(recvfromGBN(client_sock, &server_address,0,fd) == -1) {
			close(fd);
			remove("files/client/file_list.txt");
		}

		// Lettura del file contenente la lista di file del server
		end_file = lseek(fd, 0, SEEK_END);
		if (end_file >0){
			lseek(fd, 0, SEEK_SET);
			read(fd, buff, end_file);
			printf("\n--------------------- SERVER FILE LIST ---------------------\n");
			printf("%s", buff);
			printf("\n------------------------------------------------------------\n");
			close(fd);
			remove("files/client/file_list.txt");
		}
		break; // --------------------------------------------------------------------------

	case GET:
		if(sendto(client_sock, (void*)&get, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len) < 0){
			logger("ERROR", __func__, __LINE__, "Request GET failed (sendto)\n");
			exit(-1);
		}
		printf("> File name to download (%d seconds): ",SELECT_FILE_SEC);
		alarm(SELECT_FILE_SEC);
		memset(buff, 0, sizeof(buff));
		if(scanf("%s", buff)>0) {
			alarm(0);
		}
		// send filename
		if(sendto(client_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&server_address, addr_len) < 0) {
			logger("ERROR", __func__, __LINE__, "Request failed (sendto)\n");
			exit(-1);
		}
		
		snprintf(path, 13+strlen(buff)+1, "files/client/%s", buff);
		fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0666);
		// file found?
		if(recvfrom(client_sock, buff, strlen(NFOUND), 0, (struct sockaddr *)&server_address, &addr_len) < 0) {
			logger("ERROR", __func__, __LINE__, "Receive n/found failed (recvfrom)\n");
			close(fd);
			remove(path);
			free(buff);
			free(path);
			exit(-1);
		}
		if(strncmp(buff, NFOUND, strlen(NFOUND)) == 0) { //file non presente sul server se ricevo notfound
			logger("ERROR", __func__, __LINE__, "File not found \n");
			close(fd);
			remove(path);
			free(buff);
			free(path);
			exit(-1);
		}
		// receiving file
		if(recvfromGBN(client_sock, &server_address,0,fd) == -1) {
			close(fd);
			remove(path);
			logger("ERROR", __func__, __LINE__, "Error receiving file\n");
			break;
		}
		logger("INFO", __func__, __LINE__, "File received in: ");
		printf("%s\n",path);
		close(fd);
		break; // --------------------------------------------------------------------------

	case PUT:
		if(sendto(client_sock, (void*)&put, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len) < 0){
			logger("ERROR", __func__, __LINE__, "Request PUT failed (sendto)\n");
			exit(-1);
		}
		printf("> File name to upload (%d seconds): ", SELECT_FILE_SEC);
		alarm(SELECT_FILE_SEC);
		memset(buff, 0, sizeof(buff));
		if(scanf("%s", buff)>0) {
			alarm(0);
		}
		// send filename
		if(sendto(client_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&server_address, addr_len) < 0) {
			logger("ERROR", __func__, __LINE__, "Request failed (sendto)\n");
			free(buff);
			exit(-1);
		}

		snprintf(path, 13+strlen(buff)+1, "files/client/%s", buff);
		fd = open(path, O_RDONLY);
		if(fd == -1){
			logger("ERROR", __func__, __LINE__, "File not found\n");
			close(fd);
			return 1;
		}
		if(sendtoGBN(client_sock, &server_address, WINDOW, LOST_PROB, fd) == -1){
			close(fd);
			logger("ERROR", __func__, __LINE__, "Error sending file\n");
			break;
		}
		close(fd);
		logger("INFO", __func__, __LINE__, "File sended!\n");
		break; // --------------------------------------------------------------------------
	
	case CLOSE:
		if (sendto(client_sock, (void *)&close_conn, sizeof(int), 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
			logger("ERROR", __func__, __LINE__, "Error request close (sending)\n");
			exit(-1);
		}
		closeConnectionClient(client_sock, &server_address);
		return 0;
		break; // --------------------------------------------------------------------------

	case 99:
		

		for (int i = 0; i < 100; i++)
		{
			memset(buff, 0, sizeof(buff));
			sprintf(buff, "f1mec.jpg");
			snprintf(path, 13+strlen(buff)+1, "files/client/%s", buff);
			fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0666);
			if(sendto(client_sock, (void*)&get, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len) < 0){
				logger("ERROR", __func__, __LINE__, "Request GET failed (sendto)\n");
				exit(-1);
			}
			// send filename
			if(sendto(client_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&server_address, addr_len) < 0) {
				logger("ERROR", __func__, __LINE__, "Request failed (sendto)\n");
				exit(-1);
			}
			
			// file found?
			if(recvfrom(client_sock, buff, strlen(NFOUND), 0, (struct sockaddr *)&server_address, &addr_len) < 0) {
				logger("ERROR", __func__, __LINE__, "Receive n/found failed (recvfrom)\n");
				close(fd);
				remove(path);
				free(buff);
				free(path);
				exit(-1);
			}
			if(strncmp(buff, NFOUND, strlen(NFOUND)) == 0) { //file non presente sul server se ricevo notfound
				logger("ERROR", __func__, __LINE__, "File not found \n");
				close(fd);
				remove(path);
				free(buff);
				free(path);
				exit(-1);
			}
			// receiving file
			if(recvfromGBN(client_sock, &server_address,0,fd) == -1) {
				close(fd);
				remove(path);
				logger("ERROR", __func__, __LINE__, "Error receiving file\n");
				break;
			}
			//logger("INFO", __func__, __LINE__, "File received in: ");
			
		}
		close(fd);
		return 0;
		break; // --------------------------------------------------------------------------

	
	default:
		menu_choiche=0;
		printf("Not a valid\n");
		fflush(stdin);
		break;
	}
	//printf("goto init\n");
	goto init;
}




// Create client socket and bind server
int createClientSocketAndBind(int *c_sock, struct sockaddr_in *s_addr, int port) {
	
	// socket creation
	if ((*c_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        logger("ERROR", __func__, __LINE__, "Socket creation error");
		exit(-1);
	}

	// socket setup
	memset((void *)s_addr, 0, sizeof(*s_addr));
	s_addr->sin_family = AF_INET;
	s_addr->sin_port = htons(port);

	// bind
	if (inet_aton(SERVER_IP, &s_addr->sin_addr) == 0) {
		printf("CLIENT: ip conversion error\n");
		exit(-1);
	}
	//printf("BIND%s\n",&s_addr->sin_addr);
	return *c_sock;
}



// Stabilisce la connessione con il server tramite 3-way handshake
void handshakeClient (int client_sock, struct sockaddr_in *server_addr) {
	int res;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(*server_addr);

	// Invio del SYN
	printf("------------------ HANDSHAKE ------------------\n");
    logger("INFO", __func__, __LINE__, "SYN   sent        | <--- |\n");    
	res = sendto(client_sock, SYN, strlen(SYN), 0, (struct sockaddr *)server_addr, addr_len);
	if (res < 0) {
		logger("ERROR", __func__, __LINE__, "SYN failed       |  X-- |\n"); 
		exit(-1);
	}

	// In attesa del SYNACK
    logger("INFO", __func__, __LINE__, "SYNACK wait       | ---> |\n");    
	memset(buff, 0, sizeof(buff));
	res = recvfrom(client_sock, buff, strlen(SYNACK), 0, (struct sockaddr *)server_addr, &addr_len);
	if (res < 0 || strncmp(buff, SYNACK, strlen(SYNACK)) != 0) {
		logger("ERROR", __func__, __LINE__, "SYNACK failed    | --X |\n");
		exit(-1);
	}

	// Connessione stabilita
	logger("INFO", __func__,__LINE__, "ESTAB connection. |v----v|\n");
	printf("-----------------------------------------------\n");
}


// Chiude la connessione con il server in modo affidabile
void closeConnectionClient (int client_sock, struct sockaddr_in *server_addr) {
	int control;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(*server_addr);

	printf("------------------ CLOSE CONNECTION ------------------\n");
	// FIN
	logger("INFO", __func__, __LINE__, "FIN   sent               | <--- |\n"); 
	control = sendto(client_sock, FIN, strlen(FIN), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: close failed (sending FIN)\n");
		exit(-1);
	}


	// In attesa del FINACK
	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(FINACK), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, FINACK, strlen(FINACK)) != 0) {
		printf("CLIENT: close connection failed (receiving FINACK)\n");
		exit(-1);
	}
	logger("INFO", __func__, __LINE__, "FINACK received          | ---> |\n"); 


	// In attesa del FIN
	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(FIN), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, FIN, strlen(FIN)) != 0) {
		printf("CLIENT: close connection failed (receiving FIN)\n");
		exit(-1);
	}
	logger("INFO", __func__, __LINE__, "FIN received             | ---> |\n"); 
	
	// Invio del FINACK
	logger("INFO", __func__, __LINE__, "FINACK   sent            | <--- |\n");
	control = sendto(client_sock, FINACK, strlen(FINACK), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: close connection failed (sending FINACK)\n");
		exit(-1);
	}		
	
	// Connessione chiusa
	logger("INFO", __func__, __LINE__, "Connection closed        |   X  |\n");
	printf("-------------------------------------------------------\n");
}



void alarmTimeout(){
	printf("\n");
	logger("ERROR", __func__, __LINE__,"Time to make a choice expired\n"); 
	exit(-1);
}