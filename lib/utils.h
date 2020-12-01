#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>

void logger(const char* tag, const char* function, const int linenum, const char* message);
int getFiles(char *list_files[], char* dir);
int correct_send(int lost_prob) ;

void inputs_wait(char *s);
bool packet_lost(int prob);
char *timeNow();
void print_percentage(int part, int total, int oldPart);
void clearScreen();
void set_timeout_sec(int sockfd, int timeout);
void set_timeout(int, int);
int get_timeout(int sockfd);



