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
void clientReliableConnClose (int client_sock, struct sockaddr_in *server_addr);




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
	char buf[1200];

	createClientSocketAndBind(&client_sock , &server_address, SERVER_PORT);

	handshakeClient(client_sock, &server_address);
  	memset(buff, 0, sizeof(buff));

	//READY
	res = recvfrom(client_sock, buff, strlen(READY), 0, (struct sockaddr *)&server_address, &addr_len);
	if (res < 0) {
		logger("ERROR", __func__, __LINE__, "Server dispaching error\n");
		exit(-1);
	}
	

start:
	printf("\n\n============= COMMAND LIST ================\n");
	printf("1) List available files on the server\n");
	printf("2) Download a file from the server\n");
	printf("3) Upload a file to the server\n");
	printf("4) Close connection\n");
	printf("============================================\n\n");
	printf("> Choose an operation: ");
	scanf ("%d",&menu_choiche);

	switch (menu_choiche)
	{
	case LIST:
		res = sendto(client_sock, (void*)&list, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len);
		if(res < 0){
			logger("ERROR", __func__, __LINE__, "Request failed (sendto)\n");
			exit(-1);
		}
		
		int fd = open("files/client/file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666); //Apro il file con la lista dei file del server
			
		// Inizio la ricezione del file
		int control = recvfromGBN(client_sock, &server_address,0,fd);
		if(control == -1) {
			close(fd);
			remove("files/client/file_list.txt");
		}


		// Lettura del file contenente la lista di file del server
		end_file = lseek(fd, 0, SEEK_END);
		if (end_file >0){
			lseek(fd, 0, SEEK_SET);
			read(fd, buff, end_file);
			printf("\n==================== FILE LIST =====================\n");
			//printf("%s", buff);
			printf("\n====================================================\n");
			close(fd);
			//remove("files/client/file_list.txt");
		}
		break; // --------------------------------------------------------------------------

	case GET:

		break; // --------------------------------------------------------------------------

	case PUT:
		/* code */
		break; // --------------------------------------------------------------------------
	
	case CLOSE:
		/* code */
		break; // --------------------------------------------------------------------------
	
	default:
		printf("Not a valid\n");
		goto start;
		break;
	}
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
    logger("INFO", __func__, __LINE__, "SYN   sent        | <--- |");    
	res = sendto(client_sock, SYN, strlen(SYN), 0, (struct sockaddr *)server_addr, addr_len);
	if (res < 0) {
		logger("ERROR", __func__, __LINE__, "SYN failed       |  X-- |"); 
		exit(-1);
	}

	// In attesa del SYNACK
    logger("INFO", __func__, __LINE__, "SYNACK wait       | ---> |");    
	memset(buff, 0, sizeof(buff));
	res = recvfrom(client_sock, buff, strlen(SYNACK), 0, (struct sockaddr *)server_addr, &addr_len);
	if (res < 0 || strncmp(buff, SYNACK, strlen(SYNACK)) != 0) {
		logger("ERROR", __func__, __LINE__, "SYNACK failed    | --X |");
		exit(-1);
	}

	// Connessione stabilita
	logger("INFO", __func__,__LINE__, "ESTAB connection. |v----v|");
	printf("-----------------------------------------------\n");
}


// Chiude la connessione con il server in modo affidabile
void clientReliableConnClose (int client_sock, struct sockaddr_in *server_addr) {
	int control;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(*server_addr);

	printf("\n================= CONNECTION CLOSE =================\n");
	// Invio del FIN
	printf("%s CLIENT: invio FIN\n", timeNow());
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
	printf("%s CLIENT: ricevuto FINACK\n", timeNow());


	// In attesa del FIN
	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(FIN), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, FIN, strlen(FIN)) != 0) {
		printf("CLIENT: close connection failed (receiving FIN)\n");
		exit(-1);
	}
	printf("%s CLIENT: ricevuto FIN\n", timeNow());
	
	// Invio del FINACK
	printf("%s CLIENT: invio FINACK\n", timeNow());
	control = sendto(client_sock, FINACK, strlen(FINACK), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: close connection failed (sending FINACK)\n");
		exit(-1);
	}		
	
	// Connessione chiusa
	printf("CLIENT: connection closed\n");
	printf("===================================================\n\n");
}