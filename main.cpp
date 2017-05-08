/*
Author: Spencer Brydges
Class encapsulates unix socket functions
for establishing a socket on a local machine and creating a multithreaded server designed specifically for multiplayer games.
The server accepts input from any given connecting client and subsequently relays the aforementioned input to all other
connected clients. The server routinely sends out important information to the clients, including the state of other clients
(connected/disconnected), the scores of other clients etc.
If the bind port is < 1000, program must be run with elevated privileges.
*/

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h> //Link with lpthread, needed to handle simultaneous client connections
#include "server.h"

using namespace std;

GameServer g; //Make global, need to access in threaded functions whih would otherwise be impossible to achieve
bool game_active = false; //Keep track of whether a game is in progress (i.e, 2 clients connected)
bool isPlayer1 = true; //Keep track for thread function, need to relay messages through correct socket

GameServer::GameServer()
{
    lport = defaultPort;
}

GameServer::GameServer(unsigned short p)
{
    setListenPort(p);
}

void GameServer::setListenPort(unsigned short p)
{
    if(p > 0 && p < 66500) //Invalid port, can't bind...
    {
        lport = p;
    }
}

void GameServer::createSocketAndBind()
{
    local.sin_port = htons(lport); //Convert port to little or big endian depending on machine
    local.sin_family = AF_INET; //Use unix socket family
    local.sin_addr.s_addr = htonl(INADDR_ANY); //Listen on local host
    sockdescr = socket(AF_INET, SOCK_STREAM, 0); //Create socket

    if(sockdescr == -1)
    {
	perror("Error: Failed to create socket, exiting...\n");
        exit(-1);
    }

    if(bind(sockdescr, (struct sockaddr *) &local, sizeof(local)) == -1)
    {
        perror("Error: Failed to bind created socket, exiting....\n");
        exit(-1);
    }
}

bool GameServer::listenForConnections()
{
    pthread_t t1, t2; //Make threads for both players in order to send/recv simultaneously
    bool client1Connected = false;
    bool client2Connected = false;
    bool waitingForClients = true; //Know when to exit function
    int tret1, tret2 = 0; //Get return value of threads

    listen(sockdescr, 5); //Listen on established socket for clients

    while (waitingForClients) //Keep waiting for 2 clients
    {
        cout << "[+] Socket established (PORT=" << lport << "), awaiting connections..." << endl;
        int len = sizeof(struct sockaddr_in);

        if(!client1Connected)
        {
            client1descr = accept(sockdescr, (struct sockaddr *)&remote, (socklen_t *)&len);
            //sleep(5); //Give client a few seconds to initialize
            send(client1descr, "PL1\r\n", sizeof("PL1\r\n"), 0); //Let client know that it is player #1
            if(client1descr < 0)
            {
                perror("[-] Error: Failed to allow client to connect, exiting....\n");
                exit(-1);
            }
        }
        else
        {
            client2descr = accept(sockdescr, (struct sockaddr *)&remote, (socklen_t *)&len);
            //sleep(5); //Give client a few seconds to initialize
            send(client2descr, "PL2\r\n", sizeof("PL2\r\n"), 0); //Let client know that it is player #2
            if(client2descr < 0)
            {
                perror("[-] Error: Failed to allow client to connect, exiting....\n");
                exit(-1);
            }
        }

        if(!client1Connected)
        {
            cout << "[+] Client #1 connected, awaiting client 2...\n";
            client1Connected = true;
            tret1 = pthread_create(&t1, NULL, communicate, (void*)&client1descr);
        }
        else
        {
            cout << "[+] Client #2 connected, awaiting data transmission...\n";
            client2Connected = true;
            waitingForClients = false;
	    send(client1descr, "GOK\r\n", strlen("GOK\r\n"), 0);
	    send(client2descr, "GOK\r\n", strlen("GOK\r\n"), 0);
            tret2 = pthread_create(&t2, NULL, communicate, (void*)&client2descr);
            game_active = true;
            return true;
        }

    }
    return false;
}

void GameServer::relayData(const char * data, int player)
{
    if(player == 1)
        send(client2descr, data, strlen(data), 0);
    else
        send(client1descr, data, strlen(data), 0);
}

void GameServer::disconnectPlayer(int client, int player)
{
    cout << "A player has disconnected!\n\n";
    close(client);
    const char * send = "DIS\r\n";
    g.relayData(send, player);
    game_active = false;
}

GameServer::~GameServer()
{
    close(sockdescr);
}

string executePHP(char * cmd)
{
    string result = "";
    FILE * pipe = popen(cmd, "r"); //Call PHP executable with supplied user + password

    if(pipe) //Command was OK, get results
    {
		char buffer[128];
		while(!feof(pipe))
		{
			if(fgets(buffer, 128, pipe) != NULL) //...should only ever return up to 5 bits of data, but be thorough anyways
				result += buffer;
		}
		pclose(pipe);
	}

	return result;
}

string updateScore(char * username, int what)
{
    string result = "";
    char exec[256];

    strcpy(exec, phpFile);
    strcat(exec, " -u ");
    strcat(exec, username);
    strcat(exec, " -s ");
    if(what == 0) //User lost
    {
        strcat(exec, " -l ");
    }
    else //User won
    {
        strcat(exec, " -w ");
    }
    result = executePHP(exec);

    return result;
}

void * communicate(void * ptr)
{
    char recvData[256]; //Holds data sent by clients
    char message[3]; //Holds first 3 bits of data to determine what message is being sent
    char username[51]; //Holds username sent by client
    char password[33]; //Holds password sent by client

    int clientdescr = *(int *) ptr;

    int player = 0;

    if(isPlayer1)
    {
        player = 1;
        isPlayer1 = false;
    }
    else
    {
        player = 2;
    }

    while(true)
    {
        int bytesRecv = recv(clientdescr, &recvData, sizeof(recvData), 0);
        recvData[bytesRecv] = '\0'; //Nullbyte as normal
        cout << " [+] Receieved message from client: " << recvData << ", Size of message: " << bytesRecv << endl;

        g.relayData(recvData, player);

        if(bytesRecv > 4) //Clean up garbage data sent by client...need to have a proper application message
        {
            recvData[bytesRecv-2] = '\0'; //Null terminate the original message
            for(int i = 0; i < 3; i++) //Get first 3 bits off entire message to determine the context of the data
            {
                message[i] = recvData[i];
            }
            message[3] = '\0'; //Clean up buffer
        }

        if(bytesRecv <= 0) //Client has disconnected
        {
            cout << "Client terminated connection.\n";
            g.disconnectPlayer(clientdescr, player);
	    break;
        }

        if(strcmp(message, "USR") == 0)
        {
            cout << "Receiving username from client...\n";
            int index = 0;
            for(int i = 4; i < bytesRecv; i++, index++) //Get data after USR
            {
                if(recvData[i] == '\0')
                    break;
                username[index] = recvData[i];
            }
            username[++index] = '\0';
            cout << "Setting username = " << username << endl;
            send(clientdescr, "GAR\r\n", strlen("GAR\r\n"), 0); //Still need PWD which will come immediately after USR, acknowledge with GAR
        }
        else if(strcmp(message, "PWD") == 0)
        {
            cout << "Receiving password from client...\n";
            int index = 0;
            for(int i = 4; i < bytesRecv; i++, index++) //Grab data after PWD
            {
                if(recvData[i] == '\0')
                    break;
                password[index] = recvData[i];
            }
            password[++index] = '\0';
            string isGood = checkLogin(username, password); //Check login
            cout << "Got response from MySQL: \n";
            cout << isGood << endl;
            if(isGood == "\nP")
            {
                cout << " [+] Login OK\n";
                send(clientdescr, "OKL\r\n", strlen("OKL\r\n"), 0); //Login was OK, allow client to join server
            }
            else if(isGood == "\nF")
            {
                cout << " [-] Bad login\n";
                send(clientdescr, "BDL\r\n", strlen("BDL\r\n"), 0); //Bad login, client cannot join server
            }
            else //0x1A or 0x1B was returned, either way the database cannot be reached
            {
                cout << " [-] Database server is down\n";
                send(clientdescr, "DWN\r\n", strlen("DWN\r\n"), 0);
            }
            /*
            * Reset the user and password buffer to make room for retries
            */
            memset(username, 0, 51);
            memset(password, 0, 33);
        }
        else if(strcmp(message, "PUR") == 0)
        {
            cout << "Player purchased some units\n";
        }
        else if(strcmp(message, "MOV") == 0)
        {
            //Don't display this...this data will be received exponentially
        }
	else if(strcmp(message, "STR") == 0)
	{
	    cout << "Starting round...\n";
	}
      //  else if(strcmp(message, "STR") == 0)
      //  {
      //      cout << "Player #" << player << " is ready\n";
      //  }
        else if(strcmp(message, "BLD") == 0)
        {
            int separator = 0; //Location of comma separating x,y
            char tmp[10]; //Hold char bytes from i to separator for later conversion
            int x,y = 0; //Coordinates where player built tower
            cout << "Player built a tower at coordinates: ";
            /*for(int i = 4; i < bytesRecv; i++)
            {
                if(message[i] == ',')
                {
                    separator = i;
                    break;
                }
            }
            for(int i = 4; i < separator; i++)
            {
                if(i == 4)
                    strcpy(tmp, (const char *) message[i]);
                else
                    strcat(tmp, (const char *) message[i]);
            }
            x = atoi(tmp);
            tmp[0] = '\0';
            for(int i = separator; i < bytesRecv; i++)
            {
                if(i == 4)
                    strcpy(tmp, (const char *) message[i]);
                else
                    strcat(tmp, (const char *) message[i]);
            }
            y = atoi(tmp);
            cout << x << "," << y;*/
        }
        else
        {
            g.relayData("GAR\r\n", player); //Client has sent data outside of application standards, do nothing but acknowledge
        }
        cout << "Awaiting next packet...\n";
    }
}

string checkLogin(char * username, char * password)
{
	string result = "";
	char exec[256];

	cout << "Trying to login as " << username << ", password = " << password[0] << "**************************" << password[strlen(password)] << endl;

	strcpy(exec, phpFile);
	strcat(exec, " -u ");
	strcat(exec, username);
	strcat(exec, " -p ");
	strcat(exec, password);

	cout << exec << endl;

	result = executePHP(exec);
	return result;
}

int main(int argc, char **argv)
{
    g.createSocketAndBind();
    while(true)
    {
        while(!game_active)
        {
            g.listenForConnections();
        }
    }
	return 0;
}

