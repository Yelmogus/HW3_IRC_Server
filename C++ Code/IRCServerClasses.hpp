#include <set>
#include <map>
#include <string>

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
class Channel;
class UserInfo{
public:
    //Getters Returns a const reference to the private member variables
    const std::string& getName() const;
    const int getSD() const;
    const std::set<Channel>& getChannelsMemberOf() const;
    const bool getOpStatus() const;

    //Setters modifiers the private member variables
    void addChannel(Channel& newChannel);
    void removeChannel(Channel& removeChannel);
    void setName(std::string& name);
    void setSD(int SD);
    void setOpStatus(bool flag);

    bool operator<(const UserInfo& otherUser) const;
private:
    std::string name_;
    int client_sd_;
    std::set<Channel> channelmember_;
    bool isOperator_;
};

class Channel{
public:

    //Getters for the Channel object private member variable
    const std::string& getName() const;
    const std::set<UserInfo>& getUserList();
    
    //Setters that modifiers the channels private member variable
    void addUser(UserInfo& newUser);
    void removeUser(UserInfo& removeUser);
    bool operator<(const Channel& otherChannel) const;

    //Constructor for the channel object
    Channel(std::string& name);
private:
    std::string name_;
    std::set<UserInfo> userList_;
};
