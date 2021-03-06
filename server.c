#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>

#define MAX_CLIENTS 100

 
static unsigned int cli_count = 0;
static int uid = 10;
 
/* Client structure */
typedef struct {
struct sockaddr_in addr;	/* Client remote address */
int connfd;	/* Connection file descriptor */
int uid;	/* Client unique identifier */
char name[32];	/* Client name */
} client_t;
 
client_t *clients[MAX_CLIENTS];
void queue_add(client_t *cl);
void print_client_addr(struct sockaddr_in addr);
void *listening(void *argc);

/* Add client to queue */
void queue_add(client_t *cl){
int i;
for(i=0;i<MAX_CLIENTS;i++){
if(!clients[i]){
clients[i] = cl;
return;
}
}
}
 
/* Print ip address */
void print_client_addr(struct sockaddr_in addr){
printf("%d.%d.%d.%d",
addr.sin_addr.s_addr & 0xFF,
(addr.sin_addr.s_addr & 0xFF00)>>8,
(addr.sin_addr.s_addr & 0xFF0000)>>16,
(addr.sin_addr.s_addr & 0xFF000000)>>24);
}
 
/* Delete client from queue */
void queue_delete(int uid){
int i;
for(i=0;i<MAX_CLIENTS;i++){
if(clients[i]){
if(clients[i]->uid == uid){
clients[i] = NULL;
return;
}
}
}
}

/* Strip CRLF */
void strip_newline(char *s){
while(*s != '\0'){
if(*s == '\r' || *s == '\n'){
*s = '\0';
}
s++;
}
}
 
/* Send message to all clients but the sender */
void send_message(char *s, int uid){
int i;
for(i=0;i<MAX_CLIENTS;i++){
if(clients[i]){
if(clients[i]->uid != uid){
write(clients[i]->connfd, s, strlen(s));
}
}
}
}
 
/* Send message to all clients */
void send_message_all(char *s){
int i;
for(i=0;i<MAX_CLIENTS;i++){
if(clients[i]){
write(clients[i]->connfd, s, strlen(s));
}
}
}
 
/* Send message to sender */
void send_message_self(const char *s, int connfd){
write(connfd, s, strlen(s));
}
 
/* Send message to client */
void send_message_client(char *s, int uid){
int i;
int sockfd, connfd, n;
char buffer[1024];
struct sockaddr_in addr;
	strcpy(buffer, s);

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Error!\n");
		return;
	}
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(uid);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if( (connfd = connect(sockfd, ((struct sockaddr*)&addr), sizeof(addr))) < 0 )
	{
		printf("Error connect!\n");
		return;
	} 
	write(sockfd, buffer, strlen(buffer));
	close(connfd);
}

 
/* Send list of active clients */
void send_active_clients(int connfd){
int i;
char s[64];
for(i=0;i<MAX_CLIENTS;i++){
if(clients[i]){
sprintf(s, "<<CLIENT %d | %s\r\n", clients[i]->uid, clients[i]->name);
send_message_self(s, connfd);
}
}
}

void *hanle_client(void *arg){
	char buff_out[1024];
	char buff_in[1024];
	int rlen;
 
	cli_count++;
	client_t *cli = (client_t *)arg;
 
	printf("<<ACCEPT ");
	print_client_addr(cli->addr);
	printf(" REFERENCED BY %d\n", cli->uid);
 
	sprintf(buff_out, "<<JOIN, HELLO %s\r\n", cli->name);
	send_message_all(buff_out);
 
	/* Receive input from client */
	while((rlen = read(cli->connfd, buff_in, sizeof(buff_in)-1)) > 0){
        	buff_in[rlen] = '\0';
        	buff_out[0] = '\0';
		strip_newline(buff_in);
 
		/* Ignore empty buffer */
		if(!strlen(buff_in)){
			continue;
		}
 
		/* Special options */
		
		/* Send message */
		sprintf(buff_out, "[%s] %s\r\n", cli->name, buff_in);
		send_message(buff_out, cli->uid);
	
}
	 
	/* Close connection */
	close(cli->connfd);
	sprintf(buff_out, "<<LEAVE, BYE %s\r\n", cli->name);
	send_message_all(buff_out);
	 
	/* Delete client from queue and yeild thread */
	queue_delete(cli->uid);
	printf("<<LEAVE ");
	print_client_addr(cli->addr);
	printf(" REFERENCED BY %d\n", cli->uid);
	free(cli);
	cli_count--;
	pthread_detach(pthread_self());
}
void *listening(void *argc){
printf("Randu");
	int listenfd = 0, connfd = 0, port, n = 0;
struct sockaddr_in serv_addr;
struct sockaddr_in cli_addr;
    pthread_t tid;
	char buffer[1024];

	port = (int) argc;
 
/* Socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
 

    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
     perror("Socket binding failed");
     return (void *)1;
    }
 
/* Listen */
	if(listen(listenfd, 10) < 0){
		perror("Socket listening failed");
		return (void *)1;
	}

	printf("<[SERVER STARTED]>\n");

	/* Accept clients */
	while(1){
		int clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((cli_count+1) == MAX_CLIENTS){
			printf("<<MAX CLIENTS REACHED\n");
			printf("<<REJECT ");
			print_client_addr(cli_addr);
			printf("\n");
			close(connfd);
			continue;
		}
		
		read(connfd, buffer, sizeof(buffer));
		
		printf("Message: %s\n", buffer);

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->addr = cli_addr;
		cli->connfd = connfd;
		cli->uid = uid++;
		sprintf(cli->name, "%d", cli->uid);

		/* Add client to the queue and fork thread */
		queue_add(cli);
		pthread_create(&tid, NULL, &hanle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
		}
}

int main(int argc, char *argv[]){
	int port;
	pthread_t thread;
	printf("Enter your port:\n");
	scanf("%d", &port);
	pthread_create(&thread, NULL, listening, (void *)port);

	scanf("%d");

	return 0;
	
}
