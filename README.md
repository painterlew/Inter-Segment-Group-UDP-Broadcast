# Inter-Segment-Group-UDP-Broadcast

 implement a UDP multicast relay program that facilitates a user-level UDP 
 multicast environment over multiple network segments and multicast groups.
The UdpRelay object is implemented using three persistent threads (which run 
continuously while the object exists), and a relayOut threads (which run while
a connection with a remote network remains open). The three persistent threads
include: acceptsThread, commandThread, and relayInThread. The UdpRelay object
uses one instance of a Socket object, and one instance of an UdpMulticast object. 
The Socket object is utilized to help sending and receiving messages with a 
remote network, and the UdpMulticast object is utilized to help sending and 
receiving message within the local network. UdpMulticast , UdpRelay and Socket
these three standard library maps are used to set up connections.

The acceptsThread utilizes an infinite loop to wait for and accept connection 
requests from a remote network through the Socket object. When we receive a 
connection request (through Socket.getServerSocket()), the thread identifies 
the remote network and checks if a connection with that remote network already
exists. If a connection already exists, the thread will close and eliminate the
old connection before opening the new connection. The ListenToList is then 
updated and the loop continues, waiting for the next connection request.

The commandThread utilizes a loop to wait for user entered input via the keyboard 
through standard input (cin). It utilizes standard library functions to process
the command, separating it into pieces of strings when necessary, and validating
the format of the commands before calling a helper method to do the work linked
with each command. The “add” command is like the acceptsThread. Rather than 
receiving a connection, this command initiates the connection request to a remote
network. The “delete” command removes a remote connection that was previously 
added by the same UdpRelay object.

The relayInThread opens an UdpMulticast connection as the server with the local
network. Then, it utilizes an infinite loop to wait for messages to be received 
from the local network. When a message is received, it checks the message header
to determine whether the message has already been processed by the local network.
If the message is new to the local network, then the header is modified and the
message is sent to any remote networks that are connected to the UdpRelay object. 
