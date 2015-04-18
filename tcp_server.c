/******************************************************************************
 * tcp_server.c
 *
 * CPE 464 - Program 1
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "networks.h"

#define BACKLOG 5

void sendPacket();
int max_client_socket();
void tcp_receive(int);
void tcp_select();
int tcp_accept();
void initialPacketReceive(int, char *, int);
void clientMessageReceive(int, char *, int);
void sendConfirmGoodHandle(int);
void sendErrorHandle(int);
void sendMessageOk(int, int, char *);
void sendMessageError(int, int ,char *);
int handleExists(char *);


int server_socket = 0;   //socket descriptor for the server socket
int seq_num = 0;
int *client_sockets;
int client_socket_count = 0;
int client_socket_max = 10;
int port_num = 0;		 //	port number for server socket
char *buf;              //buffer for receiving from client
int buffer_size = 1024;  //packet size variable
char *active_handles[20];
int handleCount = 0;

int main(int argc, char *argv[])
{
	client_sockets = (int *)malloc(client_socket_max);
    //create packet buffer
    buf = (char *) malloc(buffer_size);
    
    //	get port_num from args if passed in
    if(argc >= 2) {
    	port_num = atoi(argv[1]);
    }

    //create the server socket
    server_socket = tcp_recv_setup(port_num);

    //look for a client to serve
    tcp_listen();
    
    while(1) {
    	tcp_select();
    }
    
    /* close the sockets */
    close(server_socket);
    int i;
    for(i = 0; i < client_socket_count; i++) {
    	close(client_sockets[i]);
    }
}

/* This function sets the server socket.  It lets the system
   determine the port number if none is set via arguments.  
   The function returns the server socket number 
   and prints the port number to the screen.  */

int tcp_recv_setup(int port_num)
{
    int server_socket = 0;
    struct sockaddr_in local;      /* socket address for local side  */
    socklen_t len = sizeof(local);  /* length of local address        */

    /* create the socket  */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0) {
      perror("socket call");
      exit(1);
    }

    local.sin_family = AF_INET;         //internet family
    local.sin_addr.s_addr = INADDR_ANY; //wild card machine address
    local.sin_port = htons(port_num);                 //let system choose the port

    /* bind the name (address) to a port */
    if (bind(server_socket, (struct sockaddr *) &local, sizeof(local)) < 0) {
		perror("bind call");
		exit(-1);
    }
    
    //get the port name and print it out
    if (getsockname(server_socket, (struct sockaddr*)&local, &len) < 0) {
		perror("getsockname call");
		exit(-1);
    }

    printf("socket has port %d \n", ntohs(local.sin_port));
	        
    return server_socket;
}

/* This function waits for a client to ask for services.  It returns
   the socket number to service the client on.    */

void tcp_listen()
{
  if (listen(server_socket, BACKLOG) < 0) {
    	perror("listen call");
    	exit(-1);
  }
}

int tcp_accept() {
  int client_socket = 0;
  if ((client_socket = accept(server_socket, (struct sockaddr*)0, (socklen_t *)0)) < 0) {
    	perror("accept call");
    	exit(-1);
  }
  
  //	Add client to array, increasing size if necessary
  client_sockets[client_socket_count++] = client_socket;
  
  if(client_socket_count == client_socket_max) {
  	client_socket_max *= 2;
  	int *temp = client_sockets;
  	client_sockets = realloc(client_sockets, client_socket_max);
  	int i;
  	for(i = 0; i < client_socket_count; i++) {
  		client_sockets[i] = temp[i];
  	}  	
  }
  
  return(client_socket);
}

int max_client_socket() {
	int max = server_socket, i;
	for(i = 0; i < client_socket_count; i++) {
		if(client_sockets[i] > max) max = client_sockets[i];
	}
	return max;
}

void tcp_select() {
	fd_set fdvar;
	int i;
	
	FD_ZERO(&fdvar); // reset variables

	//	check server socket first
	FD_SET(server_socket, &fdvar);
	
	for(i = 0; i < client_socket_count; i++) {
		FD_SET(client_sockets[i], &fdvar); 
	}
	
	if(select(max_client_socket() + 1, (fd_set *) &fdvar, NULL, NULL, NULL)  < 0) {
		perror("select call");
		exit(-1);
	}
	
	//	check server socket first
	if(FD_ISSET(server_socket, &fdvar)) {
		tcp_accept();
		tcp_listen();
	}
	
	//	check all client sockets
	for(i = 0; i < client_socket_count; i++) {
		if(FD_ISSET(client_sockets[i], &fdvar)) {
			tcp_receive(client_sockets[i]);
		}
	}
}

void tcp_receive(int client_socket) {
	int message_len;
	
	//now get the data on the client_socket
    if ((message_len = recv(client_socket, buf, buffer_size, 0)) < 0) {
		perror("recv call");
		exit(-1);
    }
    
    switch(buf[4]) {
    	case 1:
    		initialPacketReceive(client_socket, buf, message_len);
    		break;
    	case 5: 
    		clientMessageReceive(client_socket, buf, message_len);
    		break;
    	default:
    		printf("some flag\n");
    }  
}

void sendPacket(int client_socket, char *send_buf, int send_len) {
	int sent;
	
	sent = send(client_socket, send_buf, send_len, 0);
    if(sent < 0) {
        perror("send call");
		exit(-1);
    }
    
    seq_num++;

    printf("Amount of data sent is: %d\n", sent);
}

void clientMessageReceive(int client_socket, char *buf, int message_len) {
	char *clientHandle;
	int handleLength = (int) *(buf + 5);
	clientHandle = malloc(handleLength + 1); 
	
	memcpy(clientHandle, buf + 6, handleLength);
	clientHandle[handleLength] = '\0';
	
	if(handleExists(clientHandle)) {
		sendMessageOk(client_socket, handleLength, clientHandle);
	} 
	else {
		sendMessageError(client_socket, handleLength, clientHandle);
	}
}

void sendMessageOk(int client_socket, int handleLength, char *clientHandle) {
	printf("handle found – message ok\n");
	char *packet, *ptr;
	
	int packetLength = 5 + 1 + handleLength;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	memset(ptr, seq_num, 4);
	ptr += 4;
	
	memset(ptr, SERVER_MESSAGE_GOOD, 1);
	ptr += 1;
	
	memset(ptr, handleLength, 1);
	ptr += 1;
	
	memcpy(ptr, clientHandle, handleLength);
	
	sendPacket(client_socket, packet, packetLength);
}
void sendMessageError(int client_socket, int handleLength, char *clientHandle) {
	printf("handle not found – message error\n");
	char *packet, *ptr;
	
	int packetLength = 5 + 1 + handleLength;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	memset(ptr, seq_num, 4);
	ptr += 4;
	
	memset(ptr, SERVER_MESSAGE_ERROR, 1);
	ptr += 1;
	
	memset(ptr, handleLength, 1);
	ptr += 1;
	
	memcpy(ptr, clientHandle, handleLength);
	
	sendPacket(client_socket, packet, packetLength);
}

void initialPacketReceive(int client_socket, char *buf, int message_len) {
	char *clientHandle;
	int handleLength = (int) *(buf + 5);
	clientHandle = malloc(handleLength + 1);
	
	memcpy(clientHandle, buf + 6, handleLength);
	clientHandle[handleLength] = '\0';
	
	if(handleExists(clientHandle)) {
		sendErrorHandle(client_socket);
	} 
	else {
		active_handles[handleCount++] = clientHandle;
		
		sendConfirmGoodHandle(client_socket);
	}
}

int handleExists(char *handle) {
	int ndx;
	for(ndx = 0; ndx < handleCount; ndx++) {
		if(strcmp(active_handles[ndx], handle) == 0) {
			return 1;
		}
	}
	return 0;
}

void sendConfirmGoodHandle(int client_socket) {
	char *packet, *ptr;
	
	int packetLength = 5;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	memset(ptr, seq_num, 4);
	ptr += 4;
	
	memset(ptr, SERVER_INITIAL_GOOD, 1);
	
	sendPacket(client_socket, packet, packetLength);
}

void sendErrorHandle(int client_socket) {
	char *packet, *ptr;
		
	int packetLength = 5;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	memset(ptr, seq_num, 4);
	ptr += 4;
	
	memset(ptr, SERVER_INITIAL_ERROR, 1);
	
	sendPacket(client_socket, packet, packetLength);
}

