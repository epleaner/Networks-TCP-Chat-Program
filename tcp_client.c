/******************************************************************************
 * tcp_client.c
 *
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
#include "testing.h"

void tcp_receive();
void tcp_select();
void sendPacket();
void initialPacket();
void initialReceive();
void message();
void sendMessagePacket();
void sendBroadcastPacket();
void sendListPacket();
void serverMessageError(int, char *, int);
void receiveMessagePacket(int, char *, int);
void receiveBroadcastPacket(int, char *, int);
void receiveList(int, char *, int);
void receiveHandle(int);
void broadcast();
void sendExitPacket();
void clientExit();
void terminate();

char *client_handle;
char *server_name;
int sockets[2];
int seq_num = 0;
int socket_num;
char *buf;              //buffer for receiving from client
int buffer_size = 1024;  //packet size variable

int main(int argc, char * argv[])
{
    //create packet buffer
    buf = (char *) malloc(buffer_size);

    /* check command line arguments  */
    if(argc != 4) {
        printf("usage: %s handle server-name server-port\n", argv[0]);
		exit(1);
    }
	
	/*	store client handle */
	client_handle = malloc(strlen(argv[1]) + 1);
	strcpy(client_handle, argv[1]);
	
	/*	store client handle */
	server_name = malloc(strlen(argv[2]) + 1);
	strcpy(server_name, argv[2]);
	
    /* set up the socket for TCP transmission  */
    socket_num = tcp_send_setup(argv[2], argv[3]);

	//	Set up sockets to select() on: stdin and server_socket
    sockets[0] = 2;
    sockets[1] = socket_num;
        
    initialPacket();
    
    printf("$: ");
    fflush(stdout);
    
    while(1) {
    	tcp_select();
    }
        
    close(socket_num);
    return 0;
}

int tcp_send_setup(char *host_name, char *port)
{
    int socket_num;
    struct sockaddr_in remote;       // socket address for remote side
    struct hostent *hp;              // address of remote host

    // create the socket
    if ((socket_num = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    perror("socket call");
	    exit(-1);
	}

    // designate the addressing family
    remote.sin_family = AF_INET;

    // get the address of the remote host and store
    if ((hp = gethostbyname(host_name)) == NULL) {
	  printf("Error getting hostname: %s\n", host_name);
	  exit(-1);
	}
    
    memcpy((char*)&remote.sin_addr, (char*)hp->h_addr, hp->h_length);

    // get the port used on the remote side and store
    remote.sin_port= htons(atoi(port));

    if(connect(socket_num, (struct sockaddr*)&remote, sizeof(struct sockaddr_in)) < 0) {
		perror("connect call");
		exit(-1);
    }

    return socket_num;
}

void tcp_select() {
	fd_set fdvar;
	
	FD_ZERO(&fdvar); // reset variables

	FD_SET(sockets[0], &fdvar);
	FD_SET(sockets[1], &fdvar);
		
	if(select(socket_num + 1, (fd_set *) &fdvar, NULL, NULL, NULL)  < 0) {
		perror("select call");
		exit(-1);
	}
	
	if(FD_ISSET(sockets[0], &fdvar)) {
		char* input_buf = (char *) malloc(buffer_size);
    	int input_len = 0;
    	    	
    	while((input_buf[input_len] = getchar()) != '\n')
    		input_len++;
    	input_buf[input_len] = '\0';
    	
    	if(input_buf[0] != '%') {
    		printf("Invalid command\n");
    	}
    	else if(input_buf[1] == 'M' || input_buf[1] == 'm') {
    		message(input_buf);
    	}
    	else if(input_buf[1] == 'B' || input_buf[1] == 'b') {
    		broadcast(input_buf);
    		
    		printf("$: ");
    		fflush(stdout);
    	}
    	else if(input_buf[1] == 'L' || input_buf[1] == 'l') {
    		sendListPacket();
    		
    		printf("$: ");
    		fflush(stdout);
    	}
    	else if(input_buf[1] == 'E' || input_buf[1] == 'e') {
    		sendExitPacket();
    	}
    	else {
    		printf("Invalid command\n");
    	} 
	}
	
	if(FD_ISSET(sockets[1], &fdvar)) {
		tcp_receive();
	}	
}

void tcp_receive() {
	int message_len;
	
	//now get the data on the server_socket
    if ((message_len = recv(socket_num, buf, buffer_size, 0)) < 0) {
		perror("recv call");
		exit(-1);
    }
    
    if(message_len == 0) {
    	printf("Server terminated\n");
    	exit(-1);
    }
    
    switch(buf[4]) {
    	case SERVER_MESSAGE_GOOD:
    		printf("$: ");
    		fflush(stdout);
    		break;
    	case SERVER_MESSAGE_ERROR: 
    		serverMessageError(socket_num, buf, message_len);
    		
    		printf("$: ");
    		fflush(stdout);
    		break;
    	case SERVER_EXIT_ACK: 
    		clientExit();
    		break;
    	case SERVER_LIST:
    		receiveList(socket_num, buf, message_len);
    		break;
    	case CLIENT_MESSAGE:
    		receiveMessagePacket(socket_num, buf, message_len);
    		
    		printf("$: ");
    		fflush(stdout);
    		break;
    	case CLIENT_BROADCAST:
    		receiveBroadcastPacket(socket_num, buf, message_len);
    		
    		printf("$: ");
    		fflush(stdout);
    		break;
    	default:
    		printf("some other flag\n");
    }  
}

void initialPacket() {
	char *packet, *ptr;
	
	int senderHandleLen = strlen(client_handle);
	int seqNum = htons(seq_num);
	
	int packetLength = 5 + 1 + senderHandleLen;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	//memset(ptr, seq_num, 4);
	*ptr = seqNum;
	ptr += 4;
	
	memset(ptr, CLIENT_INITIAL, 1);
	ptr += 1;
	
	memset(ptr, senderHandleLen, 1);
	ptr += 1;
	
	memcpy(ptr, client_handle, senderHandleLen);
	ptr += senderHandleLen;
	
	sendPacket(packet, packetLength);
	
	initialReceive();
}

void initialReceive() {
	int message_len;
	
	//now get the data on the server_socket
    if ((message_len = recv(socket_num, buf, buffer_size, 0)) < 0) {
		perror("recv call");
		exit(-1);
    }
    
    switch(buf[4]) {
    	case 2:
    		break;
    	case 3: 
    		printf("Handle already in use: %s\n", client_handle);
    		clientExit();
    		break;
    	default:
    		printf("some other flag\n");
    }  
}

void message(char *input) {
	char *command, *handle, *text, *orig;
	int handleLength = 0;
	
	orig = malloc(strlen(input) + 1);
	
	strcpy(orig, input);
	
	command = strtok(input, " ");
	handle = strtok(NULL, " ");
	
	if(handle == NULL) {
		printf("Error, no handle given\n");
	}
	else {
		if(strtok(NULL, " ") == NULL) {
			text = "";
		}
		else {
			handleLength = strlen(handle);
			text = orig + 4 + handleLength;
		}
	
		if(strlen(text) > 1000) {
			printf("Error, message to long, message length is: %d\n", strlen(text));
			
			printf("$: ");
			fflush(stdout);
		} 
		else {
			sendMessagePacket(handle, text);
		}
	
		
	} 
}

void broadcast(char *input) {
	char *command, *text, *orig;
	
	orig = malloc(strlen(input) + 1);
	
	strcpy(orig, input);
	
	command = strtok(input, " ");
	
	if(strtok(NULL, " ") == NULL) {
		text = "";
	}
	else {
		text = orig + 3;
	}
	
	if(strlen(text) > 1000) {
		printf("Error, message to long, message length is: %d\n", strlen(text));
	}
	else {
		sendBroadcastPacket(text);
	}
}

void sendMessagePacket(char *destinationHandle, char *text) {
	char *packet, *ptr;
	
	int destHandleLen = strlen(destinationHandle);
	int senderHandleLen = strlen(client_handle);
	int textLen = strlen(text) + 1;
	int seqNum = htons(seq_num);
	
	int packetLength = 5 + 1 + destHandleLen + 1 + senderHandleLen + textLen;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	//memset(ptr, seq_num, 4);
	*ptr = seqNum;
	ptr += 4;
	
	memset(ptr, CLIENT_MESSAGE, 1);
	ptr += 1;
	
	memset(ptr, destHandleLen, 1);
	ptr += 1;
	
	memcpy(ptr, destinationHandle, destHandleLen);
	ptr += destHandleLen;
	
	memset(ptr, senderHandleLen, 1);
	ptr += 1;
	
	memcpy(ptr, client_handle, senderHandleLen);
	ptr += senderHandleLen;
	
	memcpy(ptr, text, textLen);
	
	sendPacket(packet, packetLength);
}

void serverMessageError(int socket, char *buf, int message_len) {
	char *handle;
	int handleLength = (int) *(buf + 5);
	
	handle = malloc(handleLength + 1);
	memcpy(handle, buf + 6, handleLength);
	handle[handleLength] = '\0';
	
	
	printf("Client with handle %s does not exist\n", handle);
}

void receiveMessagePacket(int socket, char *buf, int message_len) {
	char *clientHandle, *destHandle, *message;
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
		
	printf("\n%s: %s\n", clientHandle, message);
}

void receiveBroadcastPacket(int socket, char *buf, int message_len) {
	char *clientHandle, *message;
	int handleLength = (int) *(buf + 5);
		
	clientHandle = malloc(handleLength + 1); 
	
	memcpy(clientHandle, buf + 6, handleLength);
	clientHandle[handleLength] = '\0';
	
	buf += 6 + handleLength;
	
	message = malloc(message_len - 7 - handleLength);
	strcpy(message, buf);
		
	printf("\n%s: %s\n", clientHandle, message);
}

void receiveList(int socket, char *buf, int message_len) {
	int handleCount;
	buf += 5;
	
	memcpy(&handleCount, buf, 4);
	
	//printf("There are %d handles\n", handleCount);
	printf("\n");
	int count;
	for(count = 0; count < handleCount; count++) {
		receiveHandle(count);
	}
	
	printf("$: ");
	fflush(stdout);
}

void receiveHandle(int count) {
	int message_len, handleLen;
	char *handle;
	
	//now get the data on the server_socket
    if ((message_len = recv(socket_num, buf, buffer_size, 0)) < 0) {
		perror("recv call");
		exit(-1);
    }
    
    handleLen = (int) *buf;
    handle = malloc(handleLen + 1);
    memcpy(handle, buf + 1, handleLen);
    handle[handleLen] = '\0';
    printf("%s\n", handle);
}

void sendBroadcastPacket(char *text) {
	char *packet, *ptr;
	
	int senderHandleLen = strlen(client_handle);
	int textLen = strlen(text) + 1;
	int seqNum = htons(seq_num);
	
	int packetLength = 5 + 1 + senderHandleLen + textLen;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	//memset(ptr, seq_num, 4);
	*ptr = seqNum;
	ptr += 4;
	
	memset(ptr, CLIENT_BROADCAST, 1);
	ptr += 1;
	
	memset(ptr, senderHandleLen, 1);
	ptr += 1;
	
	memcpy(ptr, client_handle, senderHandleLen);
	ptr += senderHandleLen;
	
	strcpy(ptr, text);
	
	sendPacket(packet, packetLength);
}

void sendListPacket() {
	char *packet, *ptr;
	
	int seqNum = htons(seq_num);
	
	int packetLength = 5;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	//memset(ptr, seq_num, 4);
	*ptr = seqNum;
	ptr += 4;
	
	memset(ptr, CLIENT_LIST, 1);
	ptr += 1;

	sendPacket(packet, packetLength);
}

void sendExitPacket() {
	char *packet, *ptr;
	
	int seqNum = htons(seq_num);
	
	int packetLength = 5;
	
	packet = malloc(packetLength);
	ptr = packet;
		
	*ptr = seqNum;
	ptr += 4;
	
	memset(ptr, CLIENT_EXIT, 1);
	ptr += 1;

	sendPacket(packet, packetLength);
}

void sendPacket(char *send_buf, int send_len) {
	int sent;
	seq_num++;	
	
	sent = send(socket_num, send_buf, send_len, 0);
    if(sent < 0) {
        perror("send call");
		exit(-1);
    }

    //printf("Amount of data sent is: %d\n", sent);
}

void clientExit() {
	close(socket_num);
    exit(-1);
}

