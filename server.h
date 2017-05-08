/*
Author: Spencer Brydges
Definitions for class GameServer and related constants & global function definitions.
*/
#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED
#include <cstring>


/*
Bind port for server
*/

const unsigned short defaultPort = 3393;

/*
Path to PHP executable for communicating with MySQL server
*/

const char phpFile[] = "php db_handler.php";

void * communicate(void *); //Threaded function for communicating back and forth between clients
std::string checkLogin(char *, char *); //Checks login details for clients
std::string updateScore(char *, int); //Updates user's win/loss ratio, second arg = what to increment (loss or win?)
std::string executePHP(char * cmd); //Executes PHP process for database transactions


class GameServer
{
private:
    char * errorMsg; //Hold any OS-returned errors that occurred when attempting to establish a socket
    int sockdescr, client1descr, client2descr; //Socket descriptor for 2 clients & server
    struct sockaddr_in local, remote; //Structs containing local (server) and remote (client) socket information
    unsigned short lport; //Port to bind socket to. Will be set to defaultPort by default
public:
    GameServer();
    ~GameServer();
    GameServer(unsigned short);
    void createSocketAndBind(); //Creates socket
    bool listenForConnections(); //Prepares socket to listen for clients
    void setListenPort(unsigned short); //Sets port to bind socket to
    void relayData(const char *, int); //Passes messages sent by client X to client Y
    void disconnectPlayer(int, int); //Disconnects a client from the server
};

#endif // SERVER_H_INCLUDED
