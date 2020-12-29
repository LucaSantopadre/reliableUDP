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
bool packet_lost(int prob);
int getFiles(char *list_files[], char* dir);
char *timeNow();
void set_timeout_sec(int sockfd, int timeout);
void set_timeout(int, int);
int get_timeout(int sockfd);



