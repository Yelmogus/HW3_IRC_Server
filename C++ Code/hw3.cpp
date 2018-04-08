#include <string>
#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
std::string errCommand = "Command not recongized or missing arguments\nAvailalbe Commands:\n\tUSER<name>\n\tLIST[#channel]\n\tJOIN<#channel>\n\tPART[#channel]\n\tOPERATOR<password>\n\tKICK<#channel><user>\n\tPRIVMSG ( <#channel> | <user> ) <message>\n\tQUIT\n";
std::string errNotOperator = "User is not an operator, use OPERATOR <password> to elavate status\n";
std::string errWrongPass = "Incorrect Password\n";
std::string errUserID = "Invalid command, please identify yourself with USER.\n";
std::string errUserName = "Invalid Username: Please identify yourself with 20 alphanumerica characters\n";
std::string errAlreadyReg = "Invalid use of USER, you already have a name:\n";
std::string errChannelName = "Invalid ChannelName must start with '#' and be alphanumerical\n";
std::string removedFromChannel = "You have been removed from all the channels\n";

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
    //Getters Returns a const reference to the private member variables
    const std::string& getName() const {return name_;}
    const int getSD() const {return client_sd_;}
    const std::set<Channel>& getChannelsMemberOf() const {return channelmember_;}
    const bool getOpStatus() const {return isOperator_;}

    //Setters modifiers the private member variables
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

    //Getters for the Channel object private member variable
    const std::string& getName() const {return name_;}
    const std::set<UserInfo>& getUserList() {return userList_;}
    
    //Setters that modifiers the channels private member variable
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

    //Constructor for the channel object
    Channel(std::string& name){
        userList_ = std::set<UserInfo>();
        name_ = name;
    }
private:
    std::string name_;
    std::set<UserInfo> userList_;
};

bool checkUserExists(UserInfo& mUser){
    return AllUsers.find(mUser.getName()) != AllUsers.end();
}
bool checkUserExists(std::string& mUser){
    return AllUsers.find(mUser) != AllUsers.end();
}
bool checkChannelExists(Channel& mChannel){
    return AllChannels.find(mChannel.getName())!= AllChannels.end();
}
bool checkChannelExists(std::string& mChannel){
    return AllChannels.find(mChannel) != AllChannels.end();
}

bool checkUserinChannel(UserInfo& mUser, Channel& mChannel){
    if(mChannel.getUserList().find(mUser) != mChannel.getUserList().end() && 
        mUser.getChannelsMemberOf().find(mChannel) != mUser.getChannelsMemberOf().end()){
        return true;
    }
    else{
        return false;
    }
}

bool checkUserinChannel(std::string& mUser, std::string& mChannel){
    std::map<std::string, UserInfo>::iterator mUserInfo_it = AllUsers.find(mUser);
    std::map<std::string, Channel>::iterator mChannel_it = AllChannels.find(mChannel);
    if(mChannel_it == AllChannels.end() || mUserInfo_it == AllUsers.end()){
        return false;
    }
    else{
        Channel mChannel = mChannel_it->second;
        UserInfo mUser = mUserInfo_it->second;
        if (mChannel.getUserList().find(mUser) != mChannel.getUserList().end() && 
            mUser.getChannelsMemberOf().find(mChannel) != mUser.getChannelsMemberOf().end()){
            return true;
        }
        else{
            return false;
        }
    }
}

int setUpServerSocket(){
	//Set up Server Socket
    struct sockaddr_in6 server;
    int server_socket;
    int off = 0; 
    socklen_t sockaddr_len = sizeof(server);
    memset(&server, 0, sockaddr_len);

    //Set type of socket and which port is allowed
    server.sin6_addr = in6addr_any;
    server.sin6_flowinfo = 0;
    server.sin6_port = htons(0);
    server.sin6_family = AF_INET6;
    if((server_socket = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }

    //Allow dual-stack networks
    if (setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&off, sizeof(off))){
     	perror("setsockopt(IPV6_V6ONLY) failed");
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
    printf("Server listening on port: %d\n", ntohs(server.sin6_port));
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
        return -1;
    }
    else if (bytes_recv == 0){
        close(client_sd);
        exit(0);
    }
    else{
        return bytes_recv;
    }
}

void send_all(std::string channelName, std::string msg, UserInfo *mUser){
    std::map<std::string,Channel>::iterator msgChannel;
    msgChannel = AllChannels.find(channelName);

    std::set<UserInfo> users = msgChannel->second.getUserList();
    std::set<UserInfo>::const_iterator channelUser = users.begin();
    while (channelUser != users.end()){
        send(channelUser->getSD(), msg.c_str(), msg.size(), 0);
        channelUser++;
    }

}

void* handle_requests(void* args){

    pthread_detach(pthread_self());
    UserInfo *mUser = (UserInfo*) args;
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
                if(channelName.empty() || channelName[0] != '#' || !regex_match(channelName.substr(1), regexString)){ 
                    send(mUser->getSD(), errChannelName.c_str(), errChannelName.size(), 0);
                }
                else{
                    std::map<std::string,Channel>::iterator mChannel;
                    mChannel = AllChannels.find(channelName);
                     //Channel Doesnt exist make one
                    if(mChannel == AllChannels.end()){ 
                        Channel newChannel = Channel(channelName);
                        newChannel.addUser(*mUser);
                        AllChannels.insert(std::make_pair(channelName, newChannel));
                        mUser->addChannel(newChannel);

                        customMsg = "You've created a new channel " + channelName + " and joined\n";
                        send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                    }
                    //Channel was found add user to channel
                    else{
                        mChannel->second.addUser(*mUser);
                        mUser->addChannel(mChannel->second);
                        customMsg = "You've joined " + channelName + "\n";
                        send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                    }
                }
            }
        }

        //PART COMMAND - PART [#channel]
        //Remove user from specific channel if there is one or remove them from all channels
        else if(command == cmd::PART){
            //Remove user from all the channles if it is a member of that channel
            if(incomingMsg.size() == cmd::PART.size()){
                std::set<Channel> userChannels = mUser->getChannelsMemberOf();
                for(std::set<Channel>::iterator it = userChannels.begin(); it != userChannels.end(); it++){
                    std::map<std::string, Channel>::iterator channel_it = AllChannels.find(it->getName());
                    channel_it->second.removeUser(*mUser);
                }
                send(mUser->getSD(), removedFromChannel.c_str(), removedFromChannel.size(), 0);
            }
            else{
                //Remove user from the specified channel if they are part of it and its a correct name
                std::string channelName = incomingMsg.substr(cmd::PART.size()+1);
                if(channelName[0] != '#' || !regex_match(channelName.substr(1), regexString)){
                    send(mUser->getSD(), errChannelName.c_str(), errChannelName.size(), 0);
                }
                else{
                    std::map<std::string, Channel>::iterator channel_it = AllChannels.find(channelName);
                    std::set<Channel>::iterator userChannel = mUser->getChannelsMemberOf().find(channelName);
                    if(channel_it == AllChannels.end()){
                        customMsg = "No channel found with name: " + channelName + "\n";
                        send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                    }
                    else if(userChannel == mUser->getChannelsMemberOf().end()){
                        customMsg = "You are not part of channel " + channelName + ". Please use JOIN command." + "\n";
                        send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                    }
                    else{
                        channel_it->second.removeUser(*mUser);
                        std::set<Channel> mChannelUser = mUser->getChannelsMemberOf();
                        customMsg = "You've been removed from: " + channelName + "\n";
                        send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                    }
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
            else{
                send(mUser->getSD(), errWrongPass.c_str(), errWrongPass.size(), 0);
            }
        }

        //KICK COMMAND = KICK <#channel> <user>
        //Gives the user the ability to kick another user from the channel if the current user is an operator
        else if(command == cmd::KICK){
            if(mUser->getOpStatus()){
                std::string channel_user = incomingMsg.substr(command.size()+1);
                size_t space_pos = channel_user.find(' ');
                if(space_pos == std::string::npos){
                    send(mUser->getSD(), errCommand.c_str(), errCommand.size(), 0);
                }
                else{
                    std::string ChannelName = channel_user.substr(0, space_pos);
                    std::string Username = channel_user.substr(space_pos);
                    if(checkUserinChannel(Username, ChannelName)){
                        removeChannel->second.removeUser(removemUser->second);
                    }
                    else{
                        send(mUser->getSD(), errCommand.c_str(), errCommand.size(), 0);
                    }
                }
            }
            else{
                send(mUser->getSD(), errNotOperator.c_str(), errNotOperator.size(), 0);
            }
        }

        //PRIVMSG COMMAND = PRIVMSG (<#Channel> | <user>) <message>
        //Sends a message to named channel or named user at most 512 characters
        else if(command == cmd::PRIVMSG){
            
            //PRIVMSG command for a channel
            if (incomingMsg.substr(command.size() + 1, 1) == "#"){
                std::string channelName = incomingMsg.substr(command.size()+1, incomingMsg.find(' ', command.size()+1) - command.size() - 1);
                printf("%s %lu\n", channelName.c_str(), channelName.size());
                std::string msg = incomingMsg.substr(incomingMsg.find(' ', command.size()+1 + channelName.size()));
                std::map<std::string,Channel>::iterator msgChannel;
                msgChannel = AllChannels.find(channelName);
                //Check to see if channel exists
                if(msgChannel == AllChannels.end()){ 
                    customMsg = "No channel found with name: " + channelName + "\n";
                    send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                }

                std::set<Channel> memberChannels = mUser->getChannelsMemberOf();
                std::set<Channel>::iterator memberOf = memberChannels.find(channelName);

                for (memberOf = memberChannels.begin(); memberOf != memberChannels.end(); memberOf++)
                {
                    std::cout << memberOf->getName() << std::endl;
                }
                //Check to see if user is member of channel
                if (memberOf == memberChannels.end()){
                    customMsg = "You are not part of channel " + channelName + ". Please use JOIN command." + "\n";
                    send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                }

                //Message channel
                else{
                    customMsg = channelName + "> " + mUser->getName() + ": " + msg +  "\n";
                    send_all(channelName, customMsg, mUser);
                }
            }

            //PRIVMSG command for a user
            else{
                std::string userName = incomingMsg.substr(command.size()+1, incomingMsg.find(' ', command.size()+1) - command.size() - 1);
                printf("%s %lu\n", userName.c_str(), userName.size());
                std::string msg = incomingMsg.substr(incomingMsg.find(' ', command.size()+1 + userName.size()));
                std::map<std::string,UserInfo>::iterator msgUser;
                msgUser = AllUsers.find(userName);
                //Check to see if user exists
                if(msgUser == AllUsers.end()){ 
                    customMsg = "No user found with name: " + userName + "\n";
                    send(mUser->getSD(), customMsg.c_str(), customMsg.size(), 0);
                }
                //Message the user
                else{
                    customMsg = mUser->getName() + "> " + msg +  "\n";
                    send(msgUser->second.getSD(), customMsg.c_str(), customMsg.size(), 0);
                }
            }
        }

        //QUIT COMMAND = QUIT
        //Remove current user from all channels and disconnect from server
        else if (command == cmd::QUIT){
            std::set<Channel> userChannels = mUser->getChannelsMemberOf();
            for(std::set<Channel>::iterator it = userChannels.begin(); it != userChannels.end(); it++){
                std::map<std::string, Channel>::iterator channel_it = AllChannels.find(it->getName());
                channel_it->second.removeUser(*mUser);
            }
            close(mUser->getSD());
            return NULL;
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
        UserInfo* mUser = new UserInfo();
        struct sockaddr_in client;
        int client_len = sizeof( client );
        mUser->setOpStatus(false);

        printf( "SERVER: Waiting connections\n" );
        mUser->setSD(accept(server_socket, (struct sockaddr*) &client, (socklen_t*) &client_len)) ;
        printf( "SERVER: Accepted connection from %s on SockDescriptor %d\n", inet_ntoa(client.sin_addr), mUser->getSD());
        pthread_create(&tid[current_users], NULL, &handle_requests, mUser);
        std::cout << "Created thread: " << tid[current_users] << std::endl;
        current_users++; //increment user count by one
    }
}