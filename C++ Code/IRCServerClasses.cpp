#include <set>
#include <map>
#include "IRCServerClasses.hpp"
//Constant command variables
const std::string cmd::USER = "USER";
const std::string cmd::LIST = "LIST";
const std::string cmd::JOIN = "JOIN";
const std::string cmd::PART = "PART";
const std::string cmd::OPERATOR = "OPERATOR";
const std::string cmd::KICK = "KICK";
const std::string cmd::PRIVMSG = "PRIVMSG";
const std::string cmd::QUIT = "QUIT";

//Getters Returns a const reference to the private member variables
const std::string& UserInfo::getName() const {return name_;}
const int UserInfo::getSD() const {return client_sd_;}
const std::set<Channel>& UserInfo::getChannelsMemberOf() const {return channelmember_;}
const bool UserInfo::getOpStatus() const {return isOperator_;}

//Setters modifiers the private member variables
void UserInfo::addChannel(Channel& newChannel) {channelmember_.insert(newChannel);}
void UserInfo::removeChannel(Channel& removeChannel){
    std::set<Channel>::iterator it = channelmember_.find(removeChannel);
    if(it != channelmember_.end()){
        channelmember_.erase(it);
    }
}
void UserInfo::setName(std::string& name) {name_ = name;}
void UserInfo::setSD(int SD) {client_sd_ = SD;}
void UserInfo::setOpStatus(bool flag){isOperator_ = flag;}

bool UserInfo::operator<(const UserInfo& otherUser) const{
    return this->name_.compare(otherUser.name_); 
}


//Getters for the Channel object private member variable
const std::string& Channel::getName() const {return name_;}
const std::set<UserInfo>& Channel::getUserList() {return userList_;}

//Setters that modifiers the channels private member variable
void Channel::addUser(UserInfo& newUser){userList_.insert(newUser);}
void Channel::removeUser(UserInfo& removeUser){
    for (std::set<UserInfo>::iterator it = userList_.begin(); it != userList_.end(); it++){
        if (!strcmp(it->getName().c_str(), removeUser.getName().c_str())){
            userList_.erase(it);
            printf("%s removed\n", removeUser.getName().c_str());
            return;
        }
    }
    /*std::set<UserInfo>::iterator it = userList_.find(removeUser); 
    if(it != userList_.end()){
        userList_.erase(it);
        printf("USER removed\n");
    }*/
}

bool Channel::operator<(const Channel& otherChannel) const{
    return this->name_.compare(otherChannel.name_);
}
//Constructor for the channel object
Channel::Channel(std::string& name){
    userList_ = std::set<UserInfo>();
    name_ = name;
}
