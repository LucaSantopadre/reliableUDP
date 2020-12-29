#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>
#include "utils.h"
#include "common.h"



void logger(const char* tag, const char* function, const int linenum, const char* message) {
   time_t now;
   time(&now);
   printf("[%s | %s | %s:%d]: %s", timeNow(), tag, function, linenum, message);
}


// Stampa il timestamp con precisione ai microsecondi
char *timeNow(){
	struct timeval tv;
	struct tm* ptm;
	char time_string[40];
 	long microseconds;
	char *timestamp = (char *)malloc(sizeof(char) * 16);

	gettimeofday(&tv,0);
	ptm = localtime (&tv.tv_sec);
	strftime (time_string, sizeof (time_string), "%H:%M:%S", ptm);
	microseconds = tv.tv_usec;
	sprintf (timestamp,"%s.%03ld", time_string, microseconds);
	return timestamp;
}

// Set socket timeout in us
void set_timeout_sec(int sockfd, int timeout) {
	struct timeval time;
	time.tv_sec = timeout;
	time.tv_usec = 0;
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time, sizeof(time)) < 0) {
		perror("setsockopt error");
		exit(-1);
	}
}

// Set socket timeout in us
void set_timeout(int sockfd, int timeout) {
	
	struct timeval time;
	time.tv_sec = 0;
	time.tv_usec = timeout;
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time, sizeof(time)) < 0) {
		perror("setsockopt error");
		exit(-1);
	}
}



// Genera un numero casuale e ritorna true o false in base alla probabilita di perdita passata in input
bool packet_lost(int prob){
  int random = rand()%100+1;
  if (random<prob){
	  return true;
  }
  return false;
}




int getFiles(char *list_files[], char* dir) {
  /* apre la cartella e prende tutti i nomi dei file presenti in essa,
   * inserendoli in un buffer e ritornando il numero di file presenti
   */
  int i = 0;
  DIR *dp;
  struct dirent *ep;
  for(; i < MAX_FILE_LIST; ++i) {
    if ((list_files[i] = malloc(MAX_NAMEFILE_LEN * sizeof(char))) == NULL) {
      perror("malloc list_files");
      exit(EXIT_FAILURE);
    }
  }

  dp = opendir(dir);
  if(dp != NULL){
    i = 0;
    while((ep = readdir(dp))) {
      if(strncmp(ep->d_name, ".", 1) != 0 && strncmp(ep->d_name, "..", 2) != 0){
        strncpy(list_files[i], ep->d_name, MAX_NAMEFILE_LEN);
        ++i;
      }
    }
    closedir(dp);
  }else{
    perror ("Couldn't open the directory");
  }
  return i;
}