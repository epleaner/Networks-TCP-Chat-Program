#include <stdint.h>

#ifndef NETWORKSH
#define NETWORKSH

#define CLIENT_INITIAL 1
#define SERVER_INITIAL_GOOD 2
#define SERVER_INITIAL_ERROR 3
#define CLIENT_BROADCAST 4
#define CLIENT_MESSAGE 5
#define SERVER_MESSAGE_GOOD 6
#define SERVER_MESSAGE_ERROR 7
#define CLIENT_EXIT 8
#define SERVER_EXIT_ACK 9
#define CLIENT_LIST 10
#define SERVER_LIST 11

#define TEXT_MAX 1000

struct normalHeader {
	uint32_t sequenceNum;
	uint8_t flag;
};

// for the server side
int tcp_recv_setup(int);
void tcp_listen();

// for the client side
int tcp_send_setup(char *host_name, char *port);

#endif