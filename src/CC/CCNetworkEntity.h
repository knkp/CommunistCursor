#ifndef NETWORK_ENTITY_H
#define NETWORK_ENTITY_H

#include "BasicTypes.h"
#include "../Socket/SocketError.h"

#include <string>
#include <vector>
#include <memory>

/*
*
*   A CCNetworkEntity represent a computer connected to this Session. It contains the monitor information
*   about this computer as well as a way to communicate with this computer.
*
*   Note: If the socket becomes invalid all "Send" functions will fail
*
*/

struct OSEvent;

class Socket;
class CCDisplay;

class CCNetworkEntity
{
private:
    Socket* _internalSocket;
    std::vector<std::shared_ptr<CCDisplay>> displays;

private:
    // Some Helper Functions
    SocketError SendKeyEventPacket(const OSEvent& event)const;
    SocketError SendMouseEventPacket(const OSEvent& event)const;
    SocketError SendHIDEventPacket(const OSEvent& event)const;

public:
    CCNetworkEntity(Socket* socket);

    // passes buff and size to internal socket. returns a SocketError enum
    SocketError Send(const char* buff, const size_t size)const;
    // passes ToSend to the internal socket. returns a SocketError enum
    SocketError Send(const std::string toSend)const;
    // converts event into the appropriate packet and sends it over with a header
    SocketError SendOSEvent(const OSEvent& event)const;

    // This will add the display to the internal displays vector
    // class. bounds is a global virtual space bounds of this display
    void AddDisplay(std::shared_ptr<CCDisplay> display);

    // this will remove and the display passed to this function from internal list of displays
    // Entity, if not it does nothing 
    void RemoveDisplay(std::shared_ptr<CCDisplay> display);

    // this returns the display that this point coincides or NULL if there are none
    const std::shared_ptr <CCDisplay> DisplayForPoint(const Point& point)const;
    // this returns wether or not {p} is within the bounds of any of it's displays
    bool PointIntersectsEntity(const Point& p)const;
};

#endif