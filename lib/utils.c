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
   printf("[%s | %s | %s:%d]: %s\n", timeNow(), tag, function, linenum, message);
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
void setTimeout(int sockfd, int timeout) {
	struct timeval time;
	time.tv_sec = 0;
	time.tv_usec = timeout;
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time, sizeof(time)) < 0) {
		perror("setsockopt error");
		exit(-1);
	}
}




// Utilizzata per il debug e l'analisi dei pacchetti inviati
void inputs_wait(char *s){
	char c;
	printf("%s\n", s);
	while (c = getchar() != '\n');
}


// Genera un numero casuale e ritorna true o false in base alla probabilita di perdita passata in input
bool is_packet_lost(int prob){
  int random = rand() %100;
 // printf ("Random Number: %d\n",random);
  if (random<prob){
	  return true;
  }
  return false;
}





// Stampa una barra di avanzamento relativo all'invio del file
void print_percentage(int part, int total, int oldPart){
	float percentage = (float) part/total*100;
	float oldPercentage = (float) oldPart/total*100;

	if ((int) oldPercentage == (int) percentage){
		return;
	}

	printf("|");
	for (int i = 0; i<=percentage/2; i++){
		printf("█");
	}
	for (int j = percentage/2; j<50; j++){
		printf("-");
	}
	printf ("|");
	printf (" %.2f%% complete\n",percentage);
}

int correct_send(int lost_prob) {
	int randint = rand()%100+1;
	if(lost_prob<randint) {
		return 1;
	}
	return 0;
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