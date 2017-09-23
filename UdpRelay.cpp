#include "UdpRelay.h"
#include <unistd.h>
#include <algorithm>
#include <string> 


/* ---------------------- UdpRelay.cpp -------------------------
compile line: 
g++ UdpRelay.cpp UdpMulticast.cpp Socket.cpp driver.cpp ¨Co UdpRelay -lpthread

run the code: 
./udpRelay 237.255.255.255:45678
java BroadcastServer 237.255.255.255:45678

./udpRelay 238.255.255.255:45678
java BroadcastServer 238.255.255.255:45678

/udpRelay 239.255.255.255:45678
java BroadcastServer 239.255.255.255:45678
java BroadcastClient 239.255.255.255:45678 hello



Programmer's Name  Chang Liu
Course Section Number CSS503
Creation Date 6/6/2017
Date of Last Modification 6/9/2017
----------------------------------------------------------------
Purpose:
UdpRelay class an implementation of the TCP connection.
The UdpRealy enables the communication of network messages to
and from remote network groups and within local network groups.
----------------------------------------------------------------
Functionality:
Each UdpRelay note instantiates 3 threads. 
CommandThread:keeps accepting commands via user input.

acceptThread: keep listening remote connection from other relay 
node, if gets any request, set up the connection

relayInThread :keeps catching a local UDP multicast message. 
Every time relayInThread receives a UDP multicast message,
it scans the multicast header to examine if it includes the local
UdpRelay¡¯s IP address. If so, it simply discards this message. 
Otherwise forward this message through TCP connections to all 
the remote network segments/groups.

// ----------------------------------------------------------------*/

void* commandFunction(void *arg);
void* acceptFunction(void *arg);
void* relayInFunction(void *arg);
void* relayOutFunction(void *arg);


class ThreadParam {
public:

	ThreadParam(int sd, UdpRelay * relay, char * hostName)
	{
		socketDescriptor = sd;
		relayObj = relay;
		remoteHostName = hostName;
	};

	int socketDescriptor;
	UdpRelay * relayObj;
	char * remoteHostName;
};



// ------------------------UdpRelay constructor-------------------- 
// initialize all the data member and create commomad thread,
// accept thread and relay in thread 
// ----------------------------------------------------------------
UdpRelay::UdpRelay(void * arg)
{
	char* input = (char*)arg;
	// read the user input and seperate to groupIp and port two parts
	readIp_port(input, this->groupIp, groupPort);

	quit = false;

	char hostTemp[10];

	//initial hostName to all \0
	memset(hostTemp, '\0', HOST_NAME_MAX);

	//gethostname
	int result = gethostname(hostTemp, 10);

	string sHost(hostTemp);
	string sMyHost = sHost.substr(0, 10);
	myHost = sMyHost.c_str();

	// UDP initialization       
	udp = new UdpMulticast(groupIp, groupPort);
	//socket initialization 
	localSocket = new Socket(groupPort);
	
	pthread_create(&commandThread, NULL, commandFunction, (void*)this);
	pthread_create(&acceptThread, NULL, acceptFunction, (void*)this);
	pthread_create(&relayInThread, NULL, relayInFunction, (void*)this);

	pthread_join(commandThread, NULL);
	pthread_join(acceptThread, NULL);
	pthread_join(relayInThread, NULL);


	for (int i = 0; i < relayOutThreads.size(); i++)
	{
		pthread_join(*relayOutThreads[i].relayOutThread, NULL);
	}

	cout << "UdpRelay booted up at " << groupIp << ":" << groupPort << endl;
}

// -------------------------UdpRelay destructor-------------------- 
// delete all dynamic members
// ----------------------------------------------------------------
UdpRelay::~UdpRelay()
{

	delete localSocket;
	localSocket = NULL;
	delete udp;
	udp = NULL;


	delete msgMutex;
	msgMutex = NULL;
	delete cond;
	cond = NULL;

}

// ----------------------------commandFunction-------------------- 
// commandFunction which not belongs to UdpRelay class, and is used
// to call class function command
// ----------------------------------------------------------------
void* commandFunction(void *arg) {
	UdpRelay * relay = (UdpRelay*)arg;
	relay->command();
	delete relay;
}

// ----------------------------command----------------------------- 
// class function command read the user command and execute
// the command
// ----------------------------------------------------------------
void UdpRelay::command()
{
	string command;
	//std::string data = "Abc";
	string remoteInfo;
	char* remoteHostName;
	int remotePort;

	while (!quit)
	{
		//read user first part input before any space
		cin >> command;

		//convert command to all lower case
		std::transform(command.begin(), command.end(), command.begin(), ::tolower);

		if (command == "add")
		{
			//read user second part input before any space
			cin >> remoteInfo;
			if (remoteInfo.size() != 16)
			{
				cout << "invalid remote information!" << endl;
				continue;
			}

			const char* constRemoteInfo = remoteInfo.c_str();

			//read the remoteHost
			char remoteHostName[HOST_NAME_MAX];
			memset(remoteHostName, '\0', HOST_NAME_MAX);

			for (int i = 0; i < 10; i++)
			{
				remoteHostName[i] = constRemoteInfo[i];
			}

			//read the TCPPort
			char cTCPPort[5];
			for (int i = 0; i < remoteInfo.length(); i++)
			{
				cTCPPort[i] = constRemoteInfo[i + 11];
			}

			int TCPPort = atoi(cTCPPort);
			if (TCPPort < 0 || TCPPort>70000) {
				cout << "invalid TCPPort" << endl;
				continue;
			}
			addCommand(remoteHostName, TCPPort);

		}
		else if (command == "delete")
		{
			//read the remoteHost
			string sHost;
			cin >> sHost;

			//read user second part input before any space
			if (sHost.size() != 10)
			{
				cout << "invalid remote host name!" << endl;
				continue;
			}

			const char* constHost = sHost.c_str();

			char hostName[HOST_NAME_MAX];
			memset(hostName, '\0', HOST_NAME_MAX);

			for (int i = 0; i < 10; i++)
			{
				hostName[i] = constHost[i];
			}
			deleteCommand(hostName);
		}

		else if (command == "show")
		{
			showCommand();
		}
		else if (command == "help")
		{
			helpCommand();
		}
		else
		{
			cout << command << " is invalid command." << endl;
			cout << "Please use 'help' to check what commands this program provide." << endl;
			continue;
		}
	}
	// exit command thread
	pthread_exit(0);
}





// ------------------------addCommand----------------------------- 
// set up the connection to the relay node 
// add the connected fd to the sendOutList
// ----------------------------------------------------------------
bool UdpRelay::addCommand(char* remoteHost, int TCPPort)
{
	// if already listen from that remoteHost, delete the original listen 
	// connection to avoid circle connection
	if (listenToList.size() > 0)
	{
		// if already listen from that remoteHost, delete the listen connection 
		int idxL = inListenToList(remoteHost);
		if (idxL >= 0)
		{
			int sdlistenTo = listenToList[idxL].tcpSd;
			for (int i = 0; i < listenToList.size(); i++)
			{
				cout << listenToList[i].remoteHostName << ":"
					<< listenToList[i].tcpSd << endl;
			}
			
			//cancel the relayOut thread
			deleteRelayOut(sdlistenTo);
		}
	}

	// check if remoteIp already exist in sendOutList
	// if so, return true, keep the original connection
	if (inSendOutList(remoteHost) > 0)
	{
		//deleteCommand();
		return true;
	}

	else
	{
		int sd = localSocket->getClientSocket(remoteHost);
		if (sd < 0)
		{
			//error checking
			cout << "fail to add " << remoteHost << endl;
			return false;
		}
		else
		{
			
			connectionDescriptor sendOut(sd, remoteHost);
			sendOutList.push_back(sendOut);

			//send my hostName to the related node
			send(sd, myHost, HOST_NAME_MAX, 0);
			cout << "UdpRelay: added " << remoteHost << ":" << sd << endl;
			return true;
		}
	}
}




// ----------------------------commandFunction-------------------- 
// acceptFunction which not belongs to UdpRelay class, and is used
// to call class function accept
// ----------------------------------------------------------------
void* acceptFunction(void *arg)
{
	UdpRelay * relay = (UdpRelay*)arg;
	relay->accept();
}


// ----------------------------accept----------------------------- 
// class function accept which listening the connections
// when hears a conncetion save the record in listenToList 
// ----------------------------------------------------------------
void  UdpRelay::accept()
{
	int sd;
	while (true)
	{
		sd = localSocket->getServerSocket();
		if (sd > 0)
		{
			// handshake
			char remoteHost[HOST_NAME_MAX];
			recv(sd, remoteHost, HOST_NAME_MAX, 0);
			
			// if already listen from that remoteHost, delete the listen connection 
			int idxL = inListenToList(remoteHost);
			if ( idxL >= 0)
			{
				int sdlistenTo = listenToList[idxL].tcpSd;
				
				//delte the record from listenToList and relayOutThreads vector
				//cancel the relayOut thread
				deleteRelayOut(sd);
			}


			// check if exist in sendOutList which means cycle connection
			// if so, delete original send out connection
			int index = inSendOutList(remoteHost);
			
			if (index >= 0)
			{
				//close the socket descriptor to send message
				sendOutList.erase(sendOutList.begin() + index);

				int sdDelete = sendOutList[index].tcpSd;
				close(sdDelete);
			
			}

			// creat relayOut thread
			pthread_t* relayOutThread = new pthread_t;
			ThreadParam *thread_param = new ThreadParam(sd, this, remoteHost);
			pthread_create(relayOutThread, NULL, relayOutFunction, (void*)thread_param);

			relayOutWithHost relayOut_host(remoteHost, relayOutThread, sd);
			relayOutThreads.push_back(relayOut_host);
			
			connectionDescriptor listenFrom(sd, remoteHost);
			listenToList.push_back(listenFrom);


			//	pthread_join(relayOutThread, NULL);

			cout << "UdpRelay : registered " << remoteHost << endl;
		}
	}
	pthread_exit(0);

}

// ----------------------------relayInFunction-------------------- 
// relayInFunction which not belongs to UdpRelay class, and is used
// to call class function relayIn
// ----------------------------------------------------------------
void* relayInFunction(void *arg)
{
	UdpRelay * relay = (UdpRelay*)arg;
	relay->relayIn();
}

// ------------------------relayIn--------------------------------- 
//Catches UDP multicast messages from local segment network and
//check if the local segment has already seen the message by 
//examining the header. If so, the message is ignored. If not, the
//group's ip address is added to the header. 
//Finally, send the message
//to remote networks connected to this relay node.
// ----------------------------------------------------------------

void UdpRelay::relayIn()
{
	UdpMulticast * localUdp = new UdpMulticast(groupIp, groupPort);
	int localSd = localUdp->getServerSocket();
	
	if (localSd < 0)
	{
		cout << "fail to get UdpMulticast Server socket discriptor" << endl;
	}
	else
	{
		char* locMessage = new char[BUFFER_SIZE];
		// wait a message
		while (true)
		{
			int recvReturn = localUdp->recv(locMessage, BUFFER_SIZE);
			if (recvReturn > 0)
			{
				//check if the message has been read before, if so, ignre it
				if (groupIpInHeader(locMessage))
				{
					continue;
				}
				else {

					// add my groupIp to the header
					addIpToHead(locMessage);

					// send the message to the remote TCP group
					sendMsgToTCP(locMessage);
				}
			}
		}
	}
	// exit this thread
	pthread_exit(0);
}



// ----------------------------relayOutFunction-------------------- 
// relayOutFunction which not belongs to UdpRelay class, and is used
// to call class function relayOut
// ----------------------------------------------------------------
void* relayOutFunction(void *arg)
{
	ThreadParam &thread_param = *(ThreadParam*)arg;

	int sd = thread_param.socketDescriptor;
	char * remoteHostName = thread_param.remoteHostName;
	UdpRelay * relay = thread_param.relayObj;
	relay->relayOut(sd);
	//delete relay;

}


// ------------------------relayOut------------------------------ 
//keeps reading a UDP multicast message through the TCP 
//connection from remoteIp and multicasting the message 
//to the local segament
// ----------------------------------------------------------------
void UdpRelay::relayOut(int socketDescriptor)
{
	int tcpSd = socketDescriptor;

	//if (udp.getClientSocket() == NULL_SD) {
	//	error;
	//}

	char tcpMessage[BUFFER_SIZE];

	memset(tcpMessage, '\0', BUFFER_SIZE);

	while (true)
	{
		// wait message from TCP
		int recvReturn = recv(tcpSd, tcpMessage, BUFFER_SIZE, 0);
                string msgTemp(tcpMessage);
	        string sMessage = msgTemp.substr(0,6);

		if (recvReturn >= 0) {
			if (recvReturn == 0) // The cliend has closed the connection
			{
				break;
			}

			else if (sMessage == "delete")
			{
				deleteRelayOut(tcpSd);
                                  return;
			}

			//check if the message has been read before, if so, ignre it
			else if (groupIpInHeader(tcpMessage))
			{
				continue;
			}
			else
			{

                               char* tcpHost;

	                       for (int i = 0; i < listenToList.size(); i++)
	                       {
		                     if(tcpSd==listenToList[i].tcpSd)
		                     {
			                   tcpHost = listenToList[i].remoteHostName;
		                      }
	                        }
                                int MsgLen = strlen(tcpMessage);
                                cout<<"UdpRealy: received "<< MsgLen <<"bytes from "<< tcpHost  ;


				printText(tcpMessage);
				//UdpMulticast * udpRelayOut = new UdpMulticast(groupIp, groupPort);
				//UdpMulticast udprRelayOut(groupIp, groupPort);
                                cout<<"UdpRealy: BroadCast "<<  MsgLen+4 <<"bytes to "<< groupIp<<":"<< groupPort <<endl;
				int udpSd = udp->getClientSocket();
				if (udpSd > 0)
				{
					// broadcast message to local via UdpMulticast
					udp->multicast(tcpMessage);
				}
			}
		}
	}
	pthread_exit(0);
}

// --------------------------deleteRelayOut------------------------- 
//delte the record from listenToList and relayOutThreads vector
//cancel the relayOut thread
// ----------------------------------------------------------------
void UdpRelay::deleteRelayOut(int tcpSd)
{
	//delete the record from relayOutThreads
	int index = findHostInrelayOuts(tcpSd);
	if (index >=0)
	{
		pthread_t * relayOutDelete = relayOutThreads[index].relayOutThread;
		
		////cancle the relayOut thread
		//int cancelReturn = pthread_cancel(*relayOutDelete);
		//if (cancelReturn != 0)
		//{
		//	cerr << "faile to pthread_cancel() the relayOut for "
		//		<< relayOutThreads[index].remoteHostName << endl;
		//	exit(EXIT_FAILURE);
		//}
		//delete the relayOutWithHost in relayOutThreads vector
		relayOutThreads.erase(relayOutThreads.begin() + index);
		
	}

		//delte the record from listenToList
		for (int i = 0; i < listenToList.size(); i++)
	{
		if (listenToList[i].tcpSd == tcpSd)
		{
			
			listenToList.erase(listenToList.begin() + i);
		}
	}

	//close the socket descriptor to listen
	close(tcpSd);
}





// --------------------------deleteCommand------------------------- 
//Deletes the TCP connection to a remote network segment or
//group whose representative node¡¯s IP address is remoteIp.It
//also terminates the corresponding relayOutThread
// ----------------------------------------------------------------
void UdpRelay::deleteCommand(char* remoteHost)
{
	int index = inSendOutList(remoteHost);
	if (index < 0)
	{
		cout << "this host " << remoteHost << " is not currently connected!" << endl;
	}
	else
	{
		int sdDelete = sendOutList[index].tcpSd;
		const char* constDelete = "delete";

		char deleteCommand[10];
		memset(deleteCommand,'\0', 10);
		for (int i = 0; i < 6; i++)
		{
			deleteCommand[i] = constDelete[i];
		}

		//send a information to that listen relayNode to 
		//cancle that relayOut thread
		send(sdDelete, deleteCommand, 6, 0);

		sendOutList.erase(sendOutList.begin() + index);
		close(sdDelete);
	}
}



// --------------------------showCommand------------------------- 
//Shows all TCP connections to remote network segments or 
//groups.
// ----------------------------------------------------------------
void UdpRelay::showCommand()
{
	cout << "All TCP connections to remote network segments:" << endl;
	if (sendOutList.size() == 0)
		cout << "None" << endl;
	// Loop through the sendOutList and print each pair
	for (int i = 0; i < sendOutList.size(); i++)
	{
		cout << sendOutList[i].remoteHostName << ":"
			<< sendOutList[i].tcpSd << endl;
	}

	cout << "All TCP remote networks listen to:" << endl;
	if (listenToList.size() == 0)
		cout << "None" << endl;
	// Loop through the listenToList and print each pair
	for (int i = 0; i < listenToList.size(); i++)
	{
		cout << listenToList[i].remoteHostName << ":"
			<< listenToList[i].tcpSd << endl;
	}
}


// --------------------------showCommand------------------------- 
//Summarizes available commands
// ----------------------------------------------------------------
void UdpRelay::helpCommand()
{
	cout << "UdpRelay.commandThread : accepts..." << endl;
	cout << "add remoteIp : remoteTcpPort" << endl;
	cout << "delete remoteIp" << endl;
	cout << "show" << endl;
	cout << "help" << endl;
	cout << "quit" << endl;
}





bool UdpRelay::compareTwoCharArray(char* char1, char* char2)
{

	string sChar1(char1);
	string sChar2(char2);
	return (sChar1 == sChar2);
}

// ----------------------------inSendOutList------------------------- 
// reaturn true if the remoteHostNameis  in the sendOutList
// ----------------------------------------------------------------
int UdpRelay::inSendOutList(char * remoteHostName)
{
	string remoteHost(remoteHostName);
	string hostInList;
	for (int i = 0; i < sendOutList.size(); i++)
	{
		hostInList = sendOutList[i].remoteHostName;
		if (remoteHost == hostInList)
		{
			return i;
		}
	}
	return -1;
}


// -------------------------inListenToList------------------------- 
// reaturn true if the remoteHostName in the listenToList
// ----------------------------------------------------------------
int UdpRelay::inListenToList(char * remoteHostName)
{
	string remoteHost(remoteHostName);
	string hostInList;
	for (int i = 0; i < listenToList.size(); i++)
	{
		hostInList = listenToList[i].remoteHostName;
		if (remoteHost == hostInList)
		{
			return i;
		}
	}
	return -1;
}


// --------------------findHostInrelayOuts------------------------- 
// use the tcpSd to check the relayOutThreads vector, if find, 
// return the index, otherwise, return -1
// ----------------------------------------------------------------
int UdpRelay::findHostInrelayOuts(int tcpSd)
{
	for (int i = 0; i<relayOutThreads.size(); i++)
	{
		if (tcpSd == relayOutThreads[i].tcpSd)
		{
			return i;
		}
	}
	return -1;
}


// --------------------readIpFromMsg------------------------------
// Use inet_ntop() to convert bytes to ip address.
// ----------------------------------------------------------------
void UdpRelay::readIpFromMsg(const char* bytesMsg, char* ipAddr)
{
	char ipAddrStr[INET_ADDRSTRLEN];
	struct in_addr inAddr;
	memset(&ipAddrStr, 0, INET_ADDRSTRLEN);
	memset(&inAddr, 0, sizeof(inAddr));
	memcpy(&inAddr.s_addr, bytesMsg, INET_ADDRSTRLEN);

	if (inet_ntop(AF_INET, &inAddr, ipAddrStr, INET_ADDRSTRLEN) == NULL) {
		cerr << "inet_ntop() failed: bytesToAddr" << endl;
		exit(EXIT_FAILURE);
	}
	memcpy(ipAddr, ipAddrStr, INET_ADDRSTRLEN);

}


// --------------------groupIpInHeader------------------------------ 
//check header to see if this message contains my Ip, which means 
//it has been send to me, return true if already in header, 
//otherwise, false
// ----------------------------------------------------------------
bool UdpRelay::groupIpInHeader(char* msgPassIn)
{

	char* message = msgPassIn;
	int ipCount = msgPassIn[3];

	//check header return true if already in header
	for (int i = 0; i < ipCount; i++) {
		//INET_ADDRSTRLEN is 16. Length of the string form for IP.
		char eachIpAddr[INET_ADDRSTRLEN];
		readIpFromMsg(message + (4 * (i + 1)), eachIpAddr);

		if (strcmp(eachIpAddr, groupIp) == 0)
		{
			// this message has been send by me
			return true;
		}

	}
	 //The groupIp was not found in the message header
	return false;

}


// --------------------addIpToMsgHead------------------------- 
// Add the Group IP to the Message Header
// ----------------------------------------------------------------
void UdpRelay::addIpToHead(char* msgPassIn)
{
	char* message = msgPassIn;
	//get the actual size of the whole message including the header
	int msgLen = strlen(message);

	char newMessage[BUFFER_SIZE];

	//initialize the new message to all \0
	memset(newMessage, '\0', BUFFER_SIZE);
	int headerLen = 4 * (message[3] + 1);
	int textLen = msgLen - headerLen;

	// Copy first 3 constant bytes from message to newMessage
	memcpy(newMessage, message, headerLen);

	// increment hop by one
	newMessage[3]++;

	//convert groupIp to bytes
	struct in_addr ipAddr;
	memset(&ipAddr, 0, sizeof(ipAddr));
	inet_pton(AF_INET, groupIp, &ipAddr);
	char bytesIp[IP_LENGTH];
	memcpy(bytesIp, &ipAddr.s_addr, IP_LENGTH);

	char eachIpAddr1[INET_ADDRSTRLEN];
	readIpFromMsg(message + 4, eachIpAddr1);


	char eachIpAddr2[INET_ADDRSTRLEN];
	readIpFromMsg(message + 8, eachIpAddr2);

	// Insert the groupIp into the end of the modified header
	memcpy(newMessage + headerLen, bytesIp, IP_LENGTH);



	// Copy the text message
	// check if the whole message is smaller than BUFFER_SIZE
	if (msgLen < BUFFER_SIZE - 4)
	{
		memcpy(newMessage + headerLen + 4, message + headerLen, textLen);
	}
	else
	{
		int spaceLeft = BUFFER_SIZE - headerLen - 4;
		memcpy(newMessage + headerLen + 4, message + headerLen, spaceLeft);
	}

	// copy everything in newMsg back to msgPassIn
	memcpy(msgPassIn, newMessage, BUFFER_SIZE);
	char * MaybeText = msgPassIn + 12;

}


// --------------------------sendMsgToTCP------------------------- 
// Loop through the sendOutList vector and send message to each one
// ----------------------------------------------------------------
void UdpRelay::sendMsgToTCP(char* message)
{
	// Loop through the sendOutList vector and send message to each one
	int MsgLen = strlen(message);
	int tcpSd;
	for (int i = 0; i < sendOutList.size(); i++)
	{
		tcpSd = sendOutList[i].tcpSd;
		// Send the message
		if (send(tcpSd, message, MsgLen, 0) < 0) {
			cerr << "failed to send message in sendMsgToTCP" << endl;
			exit(EXIT_FAILURE);
		}
		else
		{
                     //printf("> relay %d bytes to remoteGroup[%s]\n", MsgLen, (connectionItr->first).c_str());
		}
		
	}
}


// --------------------------printText----------------------------- 
// Print out receive message size, hostName and message content
// ----------------------------------------------------------------
void UdpRelay::printText(char* tcpMessage)
{
	int msgLen = strlen(tcpMessage);

	int headerLen = 4 * (tcpMessage[3] + 1);  // i.e. startOfMessage
	// read the tcp Ip which send this message
	char tcpIp[INET_ADDRSTRLEN];
	readIpFromMsg(tcpMessage + 4, tcpIp);
	//conver the Ip address to hostName
	char  hostPassMsg[10];
	ipToHost(tcpIp, hostPassMsg);
	// read the text from message
	char* text = tcpMessage + headerLen;
	cout  << " Msg = "<<text << endl;
}



// --------------------------printText----------------------------- 
// convert ip address to hostname
// ----------------------------------------------------------------
void UdpRelay::ipToHost(char* ip, char* tcpHostName)
{
	struct in_addr inAddr;
	memset(&inAddr, 0, sizeof(inAddr));
	inet_pton(AF_INET, ip, &inAddr) != 1;
	struct hostent* host = gethostbyaddr(&inAddr, sizeof(inAddr), AF_INET);
	tcpHostName = host->h_name;
}






// ------------------------readIp_port----------------------------- 
// read the user input and seperate to groupIp and port two parts
// ----------------------------------------------------------------
bool UdpRelay::readIp_port(char*  input, char * groupIp, int& port)
{
	//get the char* type groupIp
	for (int i = 0; i < 15; i++)
	{
		groupIp[i] = input[i];
	}

	string sInput(input);

	//get the int type port
	char cPort[5];

	for (int i = 16, j = 0; i < sInput.length(); j++, i++)
	{
		cPort[j] = input[i];
	}
	port = atoi(cPort);
	return true;
}
