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

typedef struct clientStruct {
	char  *handle;
	int socket;
} client;

void sendPacket();
int max_client_socket();
void tcp_receive(int);
void tcp_select();
int tcp_accept();
void initialPacketReceive(int, char *, int);
void clientMessageReceive(int, char *, int);
void clientBroadcastReceive(int, char *, int);
void clientExitReceive(int, char *, int);
void clientListReceive(int, char *, int);
void sendConfirmGoodHandle(int);
void sendErrorHandle(int);
void sendClientMessage(char *, char *, int);
void sendBroadcastToAll(int, char *, int);
void sendList(int);
void sendListCount(int);
void sendListHandle(int, char *);
void sendMessageOk(int, int, char *);
void sendMessageError(int, int ,char *);
int handleExists(char *);
int findClient(int);
void clientExit(int);



int server_socket = 0;   //socket descriptor for the server socket
int seq_num = 0;
client *clients;
int client_socket_count = 0;
int client_socket_max = 10;
int port_num = 0;		 //	port number for server socket
char *buf;              //buffer for receiving from client
int buffer_size = 1024;  //packet size variable

int main(int argc, char *argv[])
{
	clients = (client *) malloc(sizeof(client) * client_socket_max);
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
}

/* This function sets the server socket.  It lets the system
   determine the port number if none is set via arguments.  
   The function returns the server socket number 
   and prints the port number to the screen.  */

int tcp_recv_setup(int port_num)
{
	int sk = 0;
    struct sockaddr_in local;      /* socket address for local side  */
    socklen_t len = sizeof(local);  /* length of local address        */

    /* create the socket  */
    sk = socket(AF_INET, SOCK_STREAM, 0);
    if(sk < 0) {
      perror("socket call");
      exit(1);
    }

    local.sin_family = AF_INET;         //internet family
    local.sin_addr.s_addr = INADDR_ANY; //wild card machine address
    local.sin_port = htons(port_num);                 //let system choose the port

    /* bind the name (address) to a port */
    if (bind(sk, (struct sockaddr *) &local, sizeof(local)) < 0) {
		perror("bind call");
		exit(-1);
    }
    
    //get the port name and print it out
    if (getsockname(sk, (struct sockaddr*)&local, &len) < 0) {
		perror("getsockname call");
		exit(-1);
    }

    printf("Server is using port %d\n", ntohs(local.sin_port));
	        
    return sk;
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
  clients[client_socket_count++].socket = client_socket;
    
  if(client_socket_count == client_socket_max) {
  	client_socket_max *= 2;
  	client *temp = clients;
  	clients = realloc(clients, sizeof(client) * client_socket_max);
  	int i;
  	for(i = 0; i < client_socket_count; i++) {
  		clients[i] = temp[i];
  	}  	
  }
  
  return(client_socket);
}

int max_client_socket() {
	int max = server_socket, i;
	for(i = 0; i < client_socket_count; i++) {
		if(clients[i].socket > max) max = clients[i].socket;
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
		FD_SET(clients[i].socket, &fdvar); 
	}
	
	if(select(max_client_socket() + 1, (fd_set *) &fdvar, NULL, NULL, NULL)  < 0) {
		perror("select call");
		exit(-1);
	}
	
	//	check server socket first
	if(FD_ISSET(server_socket, &fdvar)) {
		tcp_accept();
	}
	
	//	check all client sockets
	for(i = 0; i < client_socket_count; i++) {
		if(FD_ISSET(clients[i].socket, &fdvar)) {
			tcp_receive(clients[i].socket);
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
    
    if(message_len == 0) {
    	clientExit(client_socket);
    }
    else {
    	switch(buf[4]) {
    		case CLIENT_INITIAL:
    			initialPacketReceive(client_socket, buf, message_len);
    			break;
    		case CLIENT_BROADCAST:
    			clientBroadcastReceive(client_socket, buf, message_len);
    			break;
    		case CLIENT_MESSAGE: 
    			clientMessageReceive(client_socket, buf, message_len);
    			break;
    		case CLIENT_LIST:
    			clientListReceive(client_socket, buf, message_len);
    			break;
    		case CLIENT_EXIT:
    			clientExitReceive(client_socket, buf, message_len);
    			break;
    		default:
    			printf("some flag\n");
    	}  
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

    //printf("Amount of data sent is: %d\n", sent);
}

void clientMessageReceive(int client_socket, char *buf, int message_len) {
	char *clientHandle, *destHandle, *message, *orig = buf;
	int handleLength, destLength = (int) *(buf + 5);
		
	destHandle = malloc(destLength + 1); 
	
	memcpy(destHandle, buf + 6, destLength);
	destHandle[destLength] = '\0';
	
	buf += 6 + destLength;
	
	handleLength = (int) *buf;
	
	clientHandle = malloc(handleLength + 1);
	memcpy(clientHandle, buf + 1, handleLength);
	clientHandle[handleLength] = '\0';
	
	buf += 1 + handleLength;
	
	message = malloc(message_len - 7 - handleLength - destLength);
	strcpy(message, buf);
		
	if(handleExists(destHandle) > -1) {
		sendMessageOk(client_socket, handleLength, clientHandle);
		sendClientMessage(destHandle, orig, message_len);
	} 
	else {
		sendMessageError(client_socket, destLength, destHandle);
	}
}

void clientBroadcastReceive(int client_socket, char *buf, int message_len) {
	char *clientHandle, *message, *orig = buf;
	int handleLength = (int) *(buf + 5);
		
	clientHandle = malloc(handleLength + 1); 
	
	memcpy(clientHandle, buf + 6, handleLength);
	clientHandle[handleLength] = '\0';
	
	buf += 6 + handleLength;

	message = malloc(TEXT_MAX);
	strcpy(message, buf);
		
	sendBroadcastToAll(client_socket, orig, message_len);
}

void clientExitReceive(int client_socket, char *buf, int message_len) {
	char *packet, *ptr;
	
	int packetLength = 5;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	*ptr = seq_num;
	ptr += 4;
	
	*ptr = SERVER_EXIT_ACK;
		
	sendPacket(client_socket, packet, packetLength);
}

void clientListReceive(int client_socket, char *buf, int message_len) {
	sendList(client_socket);
}

void sendList(int client_socket) {
	int ndx;
	
	sendListCount(client_socket);
	
	for(ndx = 0; ndx < client_socket_count; ndx++) {
		sendListHandle(client_socket, clients[ndx].handle);
	}
}

void sendListCount(int client_socket) {
	char *packet, *ptr;
	
	int packetLength = 5 + 4;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	*ptr = seq_num;
	ptr += 4;
	
	*ptr = SERVER_LIST;
	ptr += 1;
	
	*ptr = client_socket_count;
		
	sendPacket(client_socket, packet, packetLength);
}

void sendListHandle(int client_socket, char *handle) {
	char *packet, *ptr;
	int handleLength = strlen(handle);
	
	int packetLength = 1 + handleLength;
	
	packet = malloc(packetLength);
	ptr = packet;
	
	memset(ptr, handleLength, 1);
	ptr += 1;
	
	memcpy(ptr, handle, handleLength);
	
	sendPacket(client_socket, packet, packetLength);
}

void sendClientMessage(char *dest_handle, char *packet, int packet_len) {
	int index = handleExists(dest_handle);
	int dest_socket = clients[index].socket;
	
	sendPacket(dest_socket, packet, packet_len);
}

void sendBroadcastToAll(int sender_socket, char *packet, int packet_len) {
	int ndx;
	for(ndx = 0; ndx < client_socket_count; ndx++) {
		if(clients[ndx].socket != sender_socket) {
			sendPacket(clients[ndx].socket, packet, packet_len);
		}
	}
}

void sendMessageOk(int client_socket, int handleLength, char *clientHandle) {
	char *packet, *ptr;
	
	int packetLength = 5 + 1 + handleLength;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	*ptr = seq_num;
	ptr += 4;
	
	memset(ptr, SERVER_MESSAGE_GOOD, 1);
	ptr += 1;
	
	memset(ptr, handleLength, 1);
	ptr += 1;
	
	memcpy(ptr, clientHandle, handleLength);
	
	sendPacket(client_socket, packet, packetLength);
}
void sendMessageError(int client_socket, int handleLength, char *clientHandle) {
	char *packet, *ptr;
	
	int packetLength = 5 + 1 + handleLength;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	//memset(ptr, seq_num, 4);
	*ptr = seq_num;
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
	clientHandle = (char *) malloc(handleLength + 1);
	
	memcpy(clientHandle, buf + 6, handleLength);
	clientHandle[handleLength] = '\0';
		
	if(handleExists(clientHandle) > -1) {
		sendErrorHandle(client_socket);
	} 
	else {
		int index = findClient(client_socket);
		//clients[index].handle = (char *) malloc(handleLength + 1);
		clients[index].handle = clientHandle;
		strcpy(clients[index].handle, clientHandle);
		
		sendConfirmGoodHandle(client_socket);
	}
}

int findClient(int socket) {
	int index;
	
	for(index  = 0; index < client_socket_count; index++) {
		if(clients[index].socket == socket) {
			return index;
		}
	}
	
	return -1;
}

int handleExists(char *handle) {
	int ndx;
	for(ndx = 0; ndx < client_socket_count; ndx++) {
		if(clients[ndx].handle && strcmp(clients[ndx].handle, handle) == 0) {
			return ndx;
		}
	}
	return -1;
}

void sendConfirmGoodHandle(int client_socket) {
	char *packet, *ptr;
	
	int packetLength = 5;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	//memset(ptr, seq_num, 4);
	*ptr = seq_num;
	ptr += 4;
	
	memset(ptr, SERVER_INITIAL_GOOD, 1);
	
	sendPacket(client_socket, packet, packetLength);
}

void sendErrorHandle(int client_socket) {
	char *packet, *ptr;
		
	int packetLength = 5;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	//memset(ptr, seq_num, 4);
	*ptr = seq_num;
	ptr += 4;
	
	memset(ptr, SERVER_INITIAL_ERROR, 1);
	
	sendPacket(client_socket, packet, packetLength);
}

void clientExit(int client_socket) {
	int index = findClient(client_socket);
	int i;
	
	for(i = index; i < client_socket_count + 1; i++) {
		clients[i] = clients[i + 1];
	}	
	
	client_socket_count--;	
}

