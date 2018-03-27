#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define BUFLEN 256

typedef struct UserInfo{
	char* name;
	int client_sd;
} UserInfo;


int setUpServer(){
	//Set up Server Socket
    struct sockaddr_in server;
    int server_socket;
    socklen_t sockaddr_len = sizeof(server);
    memset(&server, 0, sockaddr_len);

    //Set type of socket and which port is allowed
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(0);
    server.sin_family = PF_INET;
    if((server_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }

    //Bind the socket
    if(bind(server_socket, (struct sockaddr *)&server, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }

    //Set up the socket for listening
    if ( listen( server_socket, 5 ) == -1 ){
        perror( "listen()" );
        exit(-1);
    }

    //Prints the port number to console
    getsockname(server_socket, (struct sockaddr *)&server, &sockaddr_len);
    printf("Server listening on port: %d\n", ntohs(server.sin_port));
    return server_socket;
}

int send_wrapper(int client_sd, char* buffer, int buffer_size, int flags){
    int bytes_sent = send(client_sd, buffer, buffer_size, flags);
    if(bytes_sent < 0){
        perror("send()");
        return -1;
    }
    else{
        return bytes_sent;
    }
}

int recv_wrapper(int client_sd, char* buffer, int buffer_size, int flags){
    int bytes_recv = recv(client_sd, buffer, buffer_size, flags);
    if(bytes_recv < 0){
        perror("recv()");
        return -1;
    }
    else if (bytes_recv == 0){
        close(client_sd);
        return 0;
    }
    else{
        return bytes_recv;
    }
}

void* handle_requests(void* args){
    UserInfo* mUser = (UserInfo*) args;
    char buffer[BUFLEN];
    memset(&buffer[0], 0, BUFLEN);
    char prompt_command[] = "Enter Command=> \n";

    send(mUser->client_sd, prompt_command, strlen(prompt_command), 0);
    int bytes_recv = recv(mUser->client_sd, buffer, BUFLEN, 0);
    buffer[bytes_recv] = '\0';

    if(strstr(buffer, "USER") == NULL){
        fprintf(stderr, "Wrong command\n");
        close(mUser->client_sd);//
    }
    return NULL;
}

int main(int argc, char** kargs){

	int server_socket = setUpServer();
    int current_users = 0;
    int max_users = 10;
    pthread_t* tid = (pthread_t*) calloc(max_users, sizeof(pthread_t));

    while(1){
        if(current_users >= max_users){
            max_users *= 2;
            tid = realloc(tid, max_users);
        }
        UserInfo mUser;
        struct sockaddr_in client;
        int client_len = sizeof( client );

        printf( "SERVER: Waiting connections\n" );
        mUser.client_sd = accept(server_socket, (struct sockaddr*) &client, (socklen_t*) &client_len);
        printf( "SERVER: Accepted connection from %s on SockDescriptor %d\n", inet_ntoa(client.sin_addr), mUser.client_sd);
        close(mUser.client_sd);
        pthread_create(&tid[current_users], NULL, &handle_requests, &mUser);
        current_users++; //increment user count by one
    }
}