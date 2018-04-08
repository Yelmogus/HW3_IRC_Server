#include <string>
#include <iostream>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <getopt.h>
#include <regex>
#include <vector>
#include <set>
#include <map>
#define BUFLEN 256

//FORWARD DECLARATIONS
class Channel;
class UserInfo;

//GLOBAL VARIABLES
//READ AND WRITES MUST BE SYNCHRONIZED
std::map<std::string, Channel> AllChannels;
std::map<std::string, UserInfo> AllUsers;


//GLOBAL CONSTANTS
//Can be replaced with "^[a-z]\w{0,19}"
std::regex regexString("^[a-z][_0-9a-z]{1,19}$", std::regex_constants::icase);
std::string password = "password";

//GLOBAL ERROR MESSAGES - CONSTANTS
std::string mCommand = "Enter Command=> \n";
std::string errCommand = "Command not recongized or missing arguments: Availalbe Commands: [USER, LIST, JOIN, PART, OPERATOR, KICK, PRIVMSG, QUIT]\n";
std::string errNotOperator = "User is not an operator, use OPERATOR <password> to elavate status\n";
std::string errUserID = "Invalid command, please identify yourself with USER.\n";
std::string errUserName = "Invalid Username: Please identify yourself with 20 alphanumerica characters\n";
std::string errAlreadyReg = "Invalid use of USER, you already have a name:\n";
std::string errChannelName = "Invalid ChannelName must start with '#' and be alphanumerical\n";

//Constant command variables
class cmd{
public:
    static const std::string USER;
    static const std::string LIST;
    static const std::string JOIN;
    static const std::string PART;
    static const std::string OPERATOR;
    static const std::string KICK;
    static const std::string PRIVMSG;
    static const std::string QUIT;
};
    const std::string cmd::USER = "USER";
    const std::string cmd::LIST = "LIST";
    const std::string cmd::JOIN = "JOIN";
    const std::string cmd::PART = "PART";
    const std::string cmd::OPERATOR = "OPERATOR";
    const std::string cmd::KICK = "KICK";
    const std::string cmd::PRIVMSG = "PRIVMSG";
    const std::string cmd::QUIT = "QUIT";

class UserInfo{
public:
    std::string getName() const {return name_;}
    int getSD() {return client_sd_;}
    std::set<Channel>& getChannelsMemberOf() {return channelmember_;}
    bool getOpStatus() {return isOperator_;}

    void addChannel(Channel& newChannel) {channelmember_.insert(newChannel);}
    void setName(std::string& name) {name_ = name;}
    void setSD(int SD) {client_sd_ = SD;}
    void setOpStatus(bool flag){isOperator_ = flag;}

    bool operator<(const UserInfo& otherUser) const{
        return this->name_.compare(otherUser.name_); 
    }


private:
    std::string name_;
    int client_sd_;
    std::set<Channel> channelmember_;
    bool isOperator_;
};

class Channel{
public:
    std::string getName() const {return name_;}
    std::set<UserInfo>& getUserList() {return userList_;}
    void addUser(UserInfo& newUser){userList_.insert(newUser);}
    void removeUser(UserInfo& removeUser){
        std::set<UserInfo>::iterator it = userList_.find(removeUser);
        if(it != userList_.end()){
            userList_.erase(it);
        }
    }

    bool operator<(const Channel& otherChannel) const{
        return this->name_.compare(otherChannel.name_);
    }

    Channel(std::string& name){
        userList_ = std::set<UserInfo>();
        name_ = name;
    }
private:
    std::string name_;
    std::set<UserInfo> userList_;
};

int setUpServerSocket(){
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

void setUpServerPassword(int argc, char** kargs){
    while(1){
        static struct option long_opts[] = {
            {"opt-pass", required_argument, NULL, 'o'},
            {NULL, 0, NULL, 0}
        };
        int option_index = 0;
        int c = getopt_long(argc, kargs, "o:", long_opts, &option_index);

        if (c == -1){break;}
        else if(c == 'o'){
            password = optarg;
        }
    }
    std::cout << "Password for OPERATOR CMD is: " << password << std::endl;
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
        exit(-1);
    }
    else if (bytes_recv == 0){
        close(client_sd);
        exit(0);
    }
    else{
        return bytes_recv;
    }
}

void* handle_requests(void* args){

    pthread_detach(pthread_self());
    UserInfo* mUser = (UserInfo*) args;
    std::vector<char> buffer(BUFLEN);
    std::string incomingMsg;
    std::string customMsg;

    send(mUser->getSD(), mCommand.c_str(), mCommand.size(), 0);
    int bytes_recv = recv_wrapper(mUser->getSD(), &buffer[0], BUFLEN, 0);
    incomingMsg.append(buffer.cbegin(), buffer.cend());

    //Ask for user id first before anything else if cannot supply ID or wrong command quit imediately 
    if(incomingMsg.find(cmd::USER) == std::string::npos || incomingMsg.size() < 6){
        send(mUser->getSD(), errUserID.c_str(), errUserID.size(), 0);
        close(mUser->getSD());
        return NULL;
    }
    //First command is USER check if username is correct
    else{
        //Points to begining after "USER" keyword 5, is the first character after USER and 6 is the length of User_\n (6)
        std::string userName = incomingMsg.substr(cmd::USER.size() + 1, bytes_recv - cmd::USER.size() - 2);
        if(std::regex_match(userName, regexString)){ //USER name is correct
            customMsg = "Welcome, " + userName + "\n";
            send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
            mUser->setName(userName);
            AllUsers.insert(  std::make_pair(userName, *mUser)  );
        }
        else{ //User name is incorrect
            send(mUser->getSD(), errUserName.c_str(), errUserName.size(), 0);
            close(mUser->getSD());
            return NULL;
        }
    }

    // User has a name - verified, now awaiting further commands from user
    do{
        incomingMsg.clear();
        //buffer = std::vector<char> (BUFLEN);
        bytes_recv = recv_wrapper(mUser->getSD(), &buffer[0], BUFLEN, 0);
        incomingMsg.append(buffer.cbegin(), buffer.cbegin() + bytes_recv - 1);
        //finds the command up to the space or the entire buffer that was read in
        std::string command = incomingMsg.substr(0, incomingMsg.find(' '));

        //USER COMMAND - USER <NAME>
        if(command == cmd::USER){
            send(mUser->getSD(), errAlreadyReg.c_str(), errAlreadyReg.size(), 0);
        }

        //LIST COMMAND - LIST[#channel]
        // Display list of all available channels if no channel name, else list all users in that channel
        else if(command == cmd::LIST){
            //if the command is just LIST no space
            if(incomingMsg.size() == cmd::LIST.size()){ 
                customMsg = "There are currently " + std::to_string(AllChannels.size()) + " channels\n";
                send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                for(std::map<std::string,Channel>::iterator it = AllChannels.begin();
                    it != AllChannels.end(); it++){
                    send(mUser->getSD(), it->first.c_str(), it->first.size(), 0);
                    send(mUser->getSD(), "\n", 1, 0);
                }
            }
            //LIST command with an arugment 
            else{
                std::string channelName = incomingMsg.substr(command.size()+1);

                //Check arguement validity 
                if (channelName[0] != '#' ||  !regex_match(channelName.substr(1), regexString)){
                    send(mUser->getSD(), errChannelName.c_str(), errChannelName.size(), 0);
                }
                else{
                    //Valid channel name, look it up
                    std::map<std::string,Channel>::iterator it;
                    it = AllChannels.find(channelName);
                    if(it == AllChannels.end()){
                        customMsg = "No channel found with name: " + channelName + "\n";
                        send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                    }
                    else{
                        std::set<UserInfo> channelUsers = it->second.getUserList();
                        customMsg = "There are currently " + std::to_string(channelUsers.size()) + " members\n";
                        send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                    }
                }
            }
        }

        //JOIN COMMAND - JOIN <#channel>
        //Allow current user to join a channel, if it doesn't exist make that channel
        else if(command == cmd::JOIN){
            //JOIN command without an arugment 
            if(incomingMsg.size() == cmd::JOIN.size()){
                send(mUser->getSD(), errCommand.c_str(), errCommand.size(), 0);
            }
            //JOIN command with an arugment
            else{
                //Check arguement validity
                std::string channelName = incomingMsg.substr(cmd::JOIN.size()+1);
                if(channelName.empty() || channelName[0] != '#'){ 
                    send(mUser->getSD(), errChannelName.c_str(), errChannelName.size(), 0);
                }
                else{
                    std::map<std::string,Channel>::iterator it;
                    it = AllChannels.find(channelName);
                     //Channel Doesnt exist make one
                    if(it == AllChannels.end()){ 
                        Channel newChannel = Channel(channelName);
                        newChannel.addUser(*mUser);
                        AllChannels.insert(std::make_pair(channelName, newChannel));
                        mUser->addChannel(newChannel);
                    }
                    //Channel was found add user to channel
                    else{
                        it->second.addUser(*mUser);
                    }
                }
            }
        }

        //PART COMMAND - PART [#channel]
        //Remove user from specific channel if there is one or remove them from all channels
        else if(command == cmd::PART){
            if(incomingMsg.size() == cmd::PART.size()){
                std::set<Channel> userChannels = mUser->getChannelsMemberOf();
                for(std::set<Channel>::iterator it = userChannels.begin(); it != userChannels.end(); it++){
                    std::map<std::string, Channel>::iterator channel_it = AllChannels.find(it->getName());
                    channel_it->second.removeUser(*mUser);
                }
            }
            else{
                std::string channelName = incomingMsg.substr(cmd::PART.size()+1);
                if(channelName[0] != '#' || !regex_match(channelName.substr(1), regexString)){
                    send(mUser->getSD(), errChannelName.c_str(), errChannelName.size(), 0);
                }
                else{

                }
            }
        }

        //OPERATOR COMMAND = OPERATOR <password>
        //Gives user kick privileges if they enter the password correctly and only if the password is set from
        //the command line
        else if(command == cmd::OPERATOR){
            std::string attempt = incomingMsg.substr(cmd::OPERATOR.size() + 1);
            if(password == attempt){
                mUser->setOpStatus(true);
            }
        }

        //KICK COMMAND = KICK <#channel> <user>
        //Gives the user the ability to kick another user from the channel if the current user is an operator
        else if(command == cmd::KICK){
            if(mUser->getOpStatus()){
                std::string channel_user = incomingMsg.substr(command.size()+1);
                size_t space_pos = channel_user.find(' ');
                std::string channel = channel_user.substr(0, space_pos);
                std::string user = channel_user.substr(space_pos);

                std::map<std::string, UserInfo>::iterator removeUser = AllUsers.find(user);
                std::map<std::string, Channel>::iterator removeChannel = AllChannels.find(channel);
                removeChannel->second.removeUser(removeUser->second);
            }
            else{
                send(mUser->getSD(), errNotOperator.c_str(), errNotOperator.size(), 0);
            }
        }

        //PRIVMSG COMMAND = PRIVMSG (<#Channel> | <user>) <message>
        //Sends a message to named channel or named user at most 512 characters
        else if(command == cmd::PRIVMSG){
        }

        //QUIT COMMAND = QUIT
        //Remove current user from all channels and disconnect from server
        else if (command == cmd::QUIT){
            std::set<Channel> userChannels = mUser->getChannelsMemberOf();
            for(std::set<Channel>::iterator it = userChannels.begin(); it != userChannels.end(); it++){
                std::map<std::string, Channel>::iterator channel_it = AllChannels.find(it->getName());
                channel_it->second.removeUser(*mUser);
            }
        }
        else{
            send(mUser->getSD(), errCommand.c_str(), errCommand.size(), 0);
        }

    }while(bytes_recv > 0);

    return NULL;
}



int main(int argc, char** kargs){
    //Parses command line agruments 
    setUpServerPassword(argc, kargs);
    //Sets up server and regular expression for usernames
	int server_socket = setUpServerSocket();
    int current_users = 0;
    int max_users = 10;
    pthread_t* tid = (pthread_t*) calloc(max_users, sizeof(pthread_t));

    
    while(1){
        if(current_users >= max_users){
            max_users *= 2;
            tid = (pthread_t*) realloc(tid, max_users);
        }
        UserInfo mUser;
        struct sockaddr_in client;
        int client_len = sizeof( client );
        mUser.setOpStatus(false);

        printf( "SERVER: Waiting connections\n" );
        mUser.setSD(accept(server_socket, (struct sockaddr*) &client, (socklen_t*) &client_len)) ;
        printf( "SERVER: Accepted connection from %s on SockDescriptor %d\n", inet_ntoa(client.sin_addr), mUser.getSD());
        pthread_create(&tid[current_users], NULL, &handle_requests, &mUser);
        current_users++; //increment user count by one
    }
}