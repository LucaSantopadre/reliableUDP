#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/param.h>
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

int createSocketAndBind(int *sock, struct sockaddr_in *address, int port);
int handshake(int , struct sockaddr_in *);
void closeConnection (int server_sock, struct sockaddr_in* client_addr);




int main(int argc, char **argv){
    //socket
    printf("%lu", PAYLOAD);
    int server_sock, child_sock;
    struct sockaddr_in server_address, client_address, child_addr;

    socklen_t addr_len = sizeof(client_address);

    char *buff = calloc(PKT_SIZE, sizeof(char));
    char *path = calloc(PKT_SIZE, sizeof(char));
    char *buffToSend = calloc(PKT_SIZE, sizeof(char));

    FILE *fptr;
	pid_t pid;

    int fd;
    int res;
    int num_files;
    char *list_files[MAX_FILE_LIST];
    
    createSocketAndBind(&server_sock, &server_address, SERVER_PORT); 
    logger("INFO", __func__,__LINE__, "Server started!\n");
    while (1) {
        //printf("while\n");
        set_timeout(server_sock, 0); // recvfrom all'inizio è bloccante (si fa con timeout==0)
        if (handshake(server_sock, &client_address) == 0){   //se un client non riesce a ben connettersi, il server non forka
            pid = fork();
            
            if (pid < 0){
                logger("ERROR", __func__,__LINE__, "Fork error\n");
                // exit o return
            }
            if (pid == 0){
                pid = getpid();
                child_sock = createSocketAndBind(&child_sock, &child_addr, 0);
                set_timeout_sec(child_sock, REQUEST_SEC);
                
                //READY
                res = sendto(child_sock, READY, strlen(READY), 0, (struct sockaddr *)&client_address, addr_len);
                if (res < 0) {
                    logger("ERROR", __func__, __LINE__ ,"Port comunication failed\n");
                }


    request:
                set_timeout_sec(child_sock, REQUEST_SEC);
                logger("INFO", __func__,__LINE__, "Waiting for request...\n");
                memset(buff, 0, sizeof(buff));
                if (recvfrom(child_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len) < 0){
                    logger("ERROR", __func__, __LINE__,"Request command failed\n");
                    free(buff);
                    free(path);
                    close(child_sock);
                    return 0;
                }

                switch (*(int*)buff){

                case LIST:
                    logger("INFO", __func__, __LINE__, "LIST Request\n");
                    
                    //create file files_list.txt
                    num_files = getFiles(list_files, SERVER_DIR);
                    fd = open("file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
                    if(fd<0){
                        logger("ERROR", __func__, __LINE__, "Error opening file list");
                        close(child_sock);
                        return 1;
                    }
                    int i=0;
                    while(i<num_files) {
                        memset(buff, 0, sizeof(buff));
                        snprintf(buff, strlen(list_files[i])+2, "%s\n", list_files[i]);
                        write(fd, buff, strlen(buff));
                        i++;
                    }
                    if(sendtoGBN(child_sock, &client_address, WINDOW, LOST_PROB, fd) == -1){
                        logger("ERROR", __func__, __LINE__, "Error sending file");
                        close(fd);
                        return 1;
                    }
                    close(fd);

                    break; // -------------------------------------------------------------------------ù
                
                case GET:
                    logger("INFO", __func__, __LINE__, "GET Request\n");
                    set_timeout_sec(child_sock, SELECT_FILE_SEC);
                    memset(buff, 0, sizeof(buff));
                    // get filename
                    if (recvfrom(child_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len) < 0) {
                        logger("ERROR", __func__, __LINE__, "File transfer failed\n");
                        free(buff);
                        free(path);
                        close(child_sock);
                        return 1;
                    }
                    // open file
                    snprintf(path, 13+strlen(buff)+1, "files/server/%s", buff); 
                    fd = open(path, O_RDONLY);
                    // file not found
                    if(fd == -1){
                        logger("WARN", __func__, __LINE__, "File not found\n");
                        if (sendto(server_sock, NFOUND, strlen(NFOUND), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
                            logger("ERROR", __func__, __LINE__, "Communicate file not found error \n");
                            return 1;
                        }
                        close(fd);
                        free(buff);
                        free(path);
                        close(child_sock);
                        return 1;
                    }
                    // file ready to send
                    if (sendto(server_sock, FOUND, strlen(FOUND), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
                        logger("ERROR", __func__, __LINE__, "Communicate file found error \n");
                        return 1;
                    }
                    sendtoGBN(child_sock, &client_address, WINDOW, LOST_PROB, fd);
                    close(fd);
                    break; // -------------------------------------------------------------------------

                case PUT:
                    logger("INFO", __func__, __LINE__, "PUT Request\n");
                    set_timeout_sec(child_sock, SELECT_FILE_SEC);
                    memset(buff, 0, sizeof(buff));
                    // get filename
                    if (recvfrom(child_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len) < 0) {
                        logger("ERROR", __func__, __LINE__, "File transfer failed\n");
                        perror("");
                        free(buff);
                        close(child_sock);
                        return 1;
                    }
                    /*
                    // file alredy exists
                    snprintf(path, 13+strlen(buff)+1, "files/server/%s", buff); 
                    fd = open(path, O_RDONLY);
                    if(fd>0){
                        logger("ERROR", __func__, __LINE__, "File alredy exists\n");
                        close(fd);
                        return 1;
                    }
                    close(fd);
                    */
                    // file tranfer
                    fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0666);
                    res = recvfromGBN(child_sock, &client_address,0,fd);
                    if(res == -1) {
                        close(fd);
                        remove(path);
                        free(buff);
                        free(path);
                        close(child_sock);
                        return 1;
                    }
                    close(fd);
                    break; // -------------------------------------------------------------------------

                case CLOSE:
                    closeConnection(child_sock, &client_address);
                    free(buff);
                    free(path);
                    close(child_sock);
                    return 0;
                    break; // -------------------------------------------------------------------------


                default:
                    break;
                }
                //printf("goto request\n");
                goto request;
            } //pid==0
        } // sever_reliable ==0
    } //while(1)
    return 0;
} //main







int createSocketAndBind(int *sock, struct sockaddr_in *address, int port) {
	
	//creazione socket
	if ((*sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        logger("ERROR", __func__, __LINE__, "Socket creation error\n");
		exit(-1);
	}

	//configurazione socket
	memset((void *)address, 0, sizeof(*address));
	address->sin_family = AF_INET;
	address->sin_port = htons(port);
	address->sin_addr.s_addr = htonl(INADDR_ANY);

	// socket bind
	if (bind(*sock, (struct sockaddr *)address, sizeof(*address)) < 0) {
        logger("ERROR", __func__, __LINE__, "Socket bind error\n");
		exit(-1);
	}
	return *sock;
}


// 3-way handshake client connection
int handshake (int server_sock, struct sockaddr_in* client_addr) {
    int control;
    char *buff = calloc(PKT_SIZE, sizeof(char));
    socklen_t addr_len = sizeof(*client_addr);

    // SYN
    logger("INFO", __func__, __LINE__, "SYN wait...\n");
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, SYN, strlen(SYN)) != 0) {
        logger("ERROR", __func__, __LINE__, "connection failed (receiving SYN)\n");
        return 1;
    }

    printf("------------------ HANDSHAKE ------------------\n");
    // SYNACK
    logger("INFO", __func__,__LINE__, "SYN received      | <--- |\n");
    logger("INFO", __func__, __LINE__, "SYNACK sent       | ---> |\n");
    control = sendto(server_sock, SYNACK, strlen(SYNACK), 0, (struct sockaddr *)client_addr, addr_len);
    if (control < 0) {
        logger("ERROR", __func__, __LINE__, "connection failed (sending SYNACK)\n");
        return 1;
    }

    // ACK, ESTAB
    logger("INFO", __func__,__LINE__, "ACK received      | <--- |\n");
    logger("INFO", __func__,__LINE__, "ESTAB connection. |v----v|\n");
    printf("-----------------------------------------------\n");
    return 0;
}


// Chiude la connessione con il client in modo affidabile
void closeConnection (int server_sock, struct sockaddr_in* client_addr) {
    int res;
    char *buff = calloc(PKT_SIZE, sizeof(char));
    socklen_t addr_len = sizeof(*client_addr);
   
    printf("------------------ CLOSE CONNECTION ------------------\n");


    // FIN
    res = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (res < 0 || strncmp(buff, FIN, strlen(FIN)) != 0) {
        printf("SERVER: close connection failed (receiving FIN)\n");
        exit(-1);
    }
    logger("INFO", __func__,__LINE__, "FIN received           | <--- |\n");
 	
    // FINACK
    logger("INFO", __func__, __LINE__,"FINACK sent            | ---> |\n");
    res = sendto(server_sock, FINACK, strlen(FINACK), 0, (struct sockaddr *)client_addr, addr_len);
    if (res < 0) {
        printf("SERVER: close connection failed (sending FINACK)\n");
        exit(-1);
    }

    // FIN
    logger("INFO", __func__, __LINE__,"FIN sent               | ---> |\n");
	  res = sendto(server_sock, FIN, strlen(FIN), 0, (struct sockaddr *)client_addr, addr_len);
    if (res < 0) {
        printf("SERVER: close connection failed (sending FIN)\n");
        exit(-1);
    }

    // FINACK
    memset(buff, 0, sizeof(buff));
    res = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (res < 0 || strncmp(buff, FINACK, strlen(FINACK)) != 0) {
        printf("SERVER: close connection failed (receiving FINACK)\n");
        exit(-1);
    }

    // Connessione chiusa
    logger("INFO", __func__,__LINE__, "FINACK received        | <--- |\n");
	logger("INFO", __func__,__LINE__, "Connection closed      |   X  |\n");
	printf("-----------------------------------------------------\n");
    return;
}