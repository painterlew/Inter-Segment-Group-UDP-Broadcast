#pragma once
#include <arpa/inet.h>   // inet_addr
#include <iostream>      // cerr
#include <limits.h>      // INT_MAX
#include <pthread.h>     // pthread library
#include <unistd.h>
#include <string>
#include <strings.h>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include "UdpMulticast.h"
#include "Socket.h"
#include <netinet/in.h>   //INET_ADDRSTRLEN

using namespace std;
#define BUFFER_SIZE 1024
#define HOST_NAME_MAX 10
#define IP_LENGTH 4


class connectionDescriptor {

public:
	connectionDescriptor() 
	{
		tcpSd = -1;
		remoteHostName = "";
	}
	connectionDescriptor(int sd, char * hostName)
	{
		tcpSd = sd;
		remoteHostName = hostName;
	}

	int tcpSd;
	char *remoteHostName;
};

class relayOutWithHost {

public:
	relayOutWithHost(char * hostName, pthread_t * relayOut, int sd)
	{
		relayOutThread = relayOut;
		remoteHostName = hostName;
		tcpSd = sd;
	}
	pthread_t * relayOutThread;
	char * remoteHostName;
	int tcpSd;


	pthread_t getRelayOut() {
		return relayOutThread;
        }
	string getRemoteHost() {
		return remoteHostName;
	}

private:
	relayOutWithHost();
};


class UdpRelay
{
public:
	UdpRelay(void *arg);
	~UdpRelay();

	void command();
	void accept();
	void relayIn();
	void relayOut(int socketDescriptor);

	bool addCommand(char* remoteHost, int TCPPort);
	void deleteCommand(char* remoteHost);
	void showCommand();
	void helpCommand();
	//void quitCommand();

	UdpMulticast * udp;
	Socket * localSocket;

	pthread_cond_t* cond;
	pthread_mutex_t* msgMutex;
	char groupIp[INET_ADDRSTRLEN];
	int groupPort;

private:
	UdpRelay();
	int TCPPort = 56789;    //hard code the TCPPort
	const char *myHost;
	char msgBuffer[BUFFER_SIZE];
	bool quit;


	vector<connectionDescriptor> sendOutList;
	vector<connectionDescriptor> listenToList;

	pthread_t commandThread;
	pthread_t relayInThread;
	pthread_t acceptThread;
	vector<relayOutWithHost> relayOutThreads;

	bool readIp_port(char*  input, char * groupIp, int& port);
	int inSendOutList(char * remoteHostName);
	int inListenToList(char * remoteHostName);
	int findHostInrelayOuts(int tcpSd);
	bool compareTwoCharArray(char* char1, char* char2);
        bool groupIpInHeader(char* msgPassIn);
	void addIpToHead(char* msgPassIn);
	void readIpFromMsg(const char* bytesMsg, char* ipAddr);
	void sendMsgToTCP(char* message);
	void printText(char* tcpMessage);
	void ipToHost(char* ipAddr, char*tcpHostName);
	void deleteRelayOut(int tcpSd);


};

