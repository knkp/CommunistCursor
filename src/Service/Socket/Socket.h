/* Class that handles createing and maintaining a socket from ths OS */

#ifndef SOCKET_H
#define SOCKET_H

#include <string>
#include <vector>

#include "SocketError.h"

#ifdef _WIN32
typedef void* NativeSocketHandle;
#else
typedef int NativeSocketHandle; 
#endif

#define SOCKET_ANY_ADDRESS "ANY"

enum class SocketProtocol
{
    SOCKET_P_UDP,
    SOCKET_P_TCP,
    SOCKET_P_ANY
};

enum class SocketDisconectType
{
    SDT_SEND,
    SDT_RECEIVE,
    SDT_ALL
};

class Socket
{
private:
    static bool hasBeenInitialized; // used for checking if OSSocketStartup Was Called

    bool _isBindable; // used internally for socket options
    bool _useIPV6; // wether or not to attempt to use an IPV6 address

    void* _internalSockInfo; // used to add a little bit of efficiency 

    int port; // the port this socket is connected to
    NativeSocketHandle sfd; // changes what it represents based on OS
    bool isBound; // wether or not this socket is bound to a port
    bool isConnected; // essentially used with tcp connections
    bool isListening; // wether or not Listen has been called succesfully
    bool isBroadcast; // wether or not this is a broadcast socket
    std::string address; // the address this socket either connects to or binds to

    SocketProtocol protocol; // the current protocol to use

private:
    /* Creates a socket and stores the pointer to sfd using the class members*/
    SocketError MakeSocket();
    /* Used to make socket info for connecting, creating and binding the socket */
    SocketError MakeInternalSocketInfo();
    /* Used internally to create client sockets from Accept */
    Socket(SocketProtocol protocol, NativeSocketHandle _sfd); 
    /* Used internally */
    SocketError Accept(NativeSocketHandle* acceptedSocket);
    /* User internally with timeout NOT FULLY IMPLEMENTED*/
    SocketError Accept(NativeSocketHandle* acceptedSocket, size_t timeout);

public:
    int lastOSErr; // The last error returned by the OS that was not succesful

public:
    Socket(Socket&& socket)noexcept;
    Socket(const std::string& address, int port, bool useIPV6, SocketProtocol protocol);
    Socket(const std::string& address, int port, bool useIPV6, bool isBroadcast, SocketProtocol protocol);
    ~Socket();

    // uses address passed in constructor
    SocketError Connect();
    // can be used to change the port to connect to w/o creating a new socket
    SocketError Connect(int port);
    // can be used to change address / port to connect to w/o creating a new socket
    SocketError Connect(const std::string& address, int port = -1);
    // similiar to posix send tp but uses originally passed in address and port for destination
    SocketError SendTo(const void* bytes, size_t length);
    // similiar to posix sendto. Useful for udp sockets
    SocketError SendTo(std::string address, int port, const void* bytes, size_t length);
    // convenience function for SendTo to send string isntead of bytes
    SocketError SendTo(std::string address, int port, const std::string& toSend);
    // send bytes through connected tcp or udp socket
    SocketError Send(const void* bytes, size_t length);
    // send string through connected tcp or udp socket
    SocketError Send(const std::string& toSend);
    // Receive From Socket Similiar to posix recv
    SocketError Recv(char* buff, size_t buffLength, size_t* receivedLength);
    // Receive From Socket similiar to posix recvfrom
    SocketError RecvFrom(std::string address, int port, char* buff, size_t buffLength, size_t* receivedLength);
    // Receive From Socket similiar to posix recvfrom, uses {this->address} and {this->port} as address to receive from
    SocketError RecvFrom(char* buff, size_t buffLength, size_t* receivedLength);
    // binds to a port using {this->port} as the port to bind to
    SocketError Bind();
    // can be used to change port to bind to w/o creating a new socket
    // sets the internal socket port to {port}
    SocketError Bind(int port);
    // can be used to change address / port to bind to w/o creating a new socket
    // sets the internal address as {address} and port as {port} if port isnt -1
    SocketError Bind(const std::string& address, int port = -1);
    // Disconect socket based on SocketDisconectType
    // SDT_ALL completly disconnects both sides of socket (basically same as close without release socket)
    // SDT_SEND disconnects the "Send" side of the socket but it can still receive
    // SDT_RECEIVE disconnects the "Receive" side of the socket but it can still send
    SocketError Disconnect(SocketDisconectType sdt = SocketDisconectType::SDT_ALL);
    // allows for maximum backlog of connections as defined by the OS
    SocketError Listen();
    // any clients waiting to connect passed the {maxAwaitingConnections} will be given an error and not connected
    SocketError Listen(int maxAwaitingConnections);
    // accepts a new Client socket from incoming connections
    // you must delete {acceptSocket} when finished
    SocketError Accept(Socket** acceptedSocket);
    // NOT FULLY IMPLEMENTED WILL ALWAYS RETURN ERROR
    SocketError Accept(Socket** acceptedSocket, size_t timeout);
    // wait for server to close socket, basically used client side 
    // to wait for the server to finish using the socket, really only matters for tcp sockets
    SocketError WaitForServer();
    // closes the socket by calling closesocket or simliar function from OS
    // if {reCreate} is true, the socket will be created again in a way 
    // that allows the socket to be used again
    SocketError Close(bool reCreate = false);

    // Setters

    // sets the stats of this socket to be able to multicast if true or disables them if false
    SocketError SetIsBroadcastable(bool);

    // Getters

    inline bool GetIsListening()const { return isListening; }
    inline bool GetIsBound()const { return isBound; }
    inline bool GetIsConnected()const { return isConnected; }
    inline bool GetIsBradcastable()const { return isBroadcast; }
    inline bool GetCanUseIPV6()const {return _useIPV6;}
    inline const std::string& GetAddress()const { return address; }
    inline const int GetPort()const { return port; }

    /* this must be called before any sockets are created */
    static void OSSocketStartup();
    /* this must be called when there is no longer a need for sockets */
    static void OSSocketTeardown();
};

#endif
