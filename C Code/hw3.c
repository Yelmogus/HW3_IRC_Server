#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
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
#include <regex.h>
#define BUFLEN 256


//GLOBAL CONSTANTS
regex_t regexCompiled;

typedef struct UserInfo{
	char* name;
	int client_sd;
} UserInfo;

typdef struct Channel{
    char* name;
    char** userList;
    int numUsers;
} Channel;


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


void setUpRegExpression(regex_t* regexCompiled, char* regexString){
    //Attempts to Match the user entered name to a regular expression
    int errCode = regcomp(regexCompiled, regexString, REG_ICASE);
    if(errCode){
        regfree(regexCompiled);
        size_t errMsgLength = regerror(errCode, regexCompiled, NULL, 0);
        char* buffer = (char*)malloc(errMsgLength);
        (void) regerror(errCode, regexCompiled, buffer, errMsgLength);
        fprintf(stderr, "%s\n", buffer);
        free(buffer);
        exit(1);
    }
}

int matchRegExpression(regex_t* regexCompiled, char* stringTest){
    printf("%s\n", stringTest);
    int errCode = regexec(regexCompiled, stringTest, 0, NULL, 0); 
    // 0 number of matches - dont care, 
    //NUll array to store matched subexpressions
    // 0 - for cflags  
    if(errCode){
        //printf("Error no matches found\n");
        size_t errMsgLength = regerror(errCode, regexCompiled, NULL, 0);
        char* buffer = (char*)malloc(errMsgLength);
        (void) regerror(errCode, regexCompiled, buffer, errMsgLength);
        fprintf(stderr, "%s\n", buffer);
        free(buffer);
    }
    //Match was found
    return errCode;
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
    pthread_detach(pthread_self());
    UserInfo* mUser = (UserInfo*) args;
    char buffer[BUFLEN];
    memset(&buffer[0], 0, BUFLEN);
    char mCommand[] = "Enter Command=> \n";
    char errMsg[] = "Invalid command, please identify yourself with USER.\n";
    char customMsg[BUFLEN];

    send(mUser->client_sd, mCommand, strlen(mCommand), 0);
    int bytes_recv = recv(mUser->client_sd, buffer, BUFLEN, 0);
    buffer[bytes_recv] = '\0';
    char* userName = strstr(buffer, "USER");
    if(userName == NULL || strlen(buffer) < 6){
        send(mUser->client_sd, errMsg, strlen(errMsg), 0);
        close(mUser->client_sd);//
    }
    else{
        userName += 5; //Points to begining after "USER" keyword
        int matches = matchRegExpression(&regexCompiled, userName);
        if(matches == 0){
            sprintf(&customMsg[0], "Welcome, %s\n", userName);
            send(mUser->client_sd, customMsg, strlen(customMsg), 0);
        }
        else{
            printf("Invalid username! %s\n", userName);
        }
    }
    return NULL;
}


int main(int argc, char** kargs){

    //Sets up server and regular expression for usernames
	int server_socket = setUpServer();
    char* regexString = "^[a-z][_0-9a-z]";
    setUpRegExpression(&regexCompiled, regexString);

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
        pthread_create(&tid[current_users], NULL, &handle_requests, &mUser);
        current_users++; //increment user count by one
    }
}