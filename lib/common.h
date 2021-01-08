#include <stdint.h>

char i;
#define fflush(stdin) while ((i = getchar()) != '\n' && i != EOF) { };

// CONNECTION
#define SERVER_PORT 25440
#define SERVER_IP "127.0.0.1"

// MSGS
#define SYN "syn"
#define SYNACK "synack"
#define ACK_SYNACK "ack_synack"
#define FIN "fin"
#define FINACK "ackfin"
#define FOUND "found"
#define NFOUND "notfound"
#define NOVERW "nooverwr"
#define READY "ready"

// COMMANDS
#define LIST 1
#define GET 2
#define PUT 3
#define CLOSE 4

// TX PARAMS
#define LOST_PROB 2			// 0%<=LOST_PROB<=100%
#define WINDOW 4			// Dimensione della finestra di trasmissione
#define PKT_SIZE 1500			// Dimensione del pacchetto
#define MAX_ERR 25

// TIMEOUT PARAMS IN USEC
#define TIMEOUT_PKT 8000	// valore timeout 4 millisecondi
#define TIME_UNIT 4000		//valore minimo di cui si può variare timeout
#define MAX_TIMEOUT 400000  //400 millisecondi timeout massimo scelto
#define MIN_TIMEOUT 4000	//4 millisecondi timeout minimo
#define ADAPTIVE 0			//impostare a 0 per abolire timeout adattativo

// TIMEOUT PARAMS IN SEC
#define REQUEST_SEC 20
#define SELECT_FILE_SEC 40

// CLIENT/SERVER DIRS
#define CLIENT_DIR "./files/client"
#define SERVER_DIR "./files/server"


// FILES PARAMS
#define MAX_FILE_LIST 100		// Massimo numero di file mostrati nella lista
#define MAX_NAMEFILE_LEN 127	// Massimo numero di caratteri mostrato nel filename

// PKT DEFINITION
#define PAYLOAD ( PKT_SIZE-sizeof(int)-sizeof(short int) )
typedef struct packet{
	int seq_num;
	short int pkt_dim;
	char data[PAYLOAD];
} packet;