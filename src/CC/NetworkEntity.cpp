#include "NetworkEntity.h"

#include "../Socket/Socket.h"
#include "../OSInterface/OSTypes.h"
#include "../OSInterface/PacketTypes.h"

int NetworkEntity::SendKeyEventPacket(const OSEvent& event)const
{
    KeyEventPacket packet(event);
    EventPacketHeader header(EVENT_PACKET_K);

    int ret = _internalSocket->Send(&header, sizeof(header));
    if(ret != SOCKET_E_SUCCESS)
        return ret;
    return _internalSocket->Send(&packet, sizeof(packet));
}

int NetworkEntity::SendMouseEventPacket(const OSEvent& event)const
{
    switch(event.subEvent.mouseEvent)
    {
        case MOUSE_EVENT_DOWN:
        case MOUSE_EVENT_UP:
            {
                EventPacketHeader header(EVENT_PACKET_MB);
                MouseButtonEventPacket packet(event);

                int ret = _internalSocket->Send(&header, sizeof(header));
                if(ret != SOCKET_E_SUCCESS)
                    return ret;
                return _internalSocket->Send(&packet, sizeof(packet));
            }
            break;
        case MOUSE_EVENT_SCROLL:
            {
                EventPacketHeader header(EVENT_PACKET_MW);
                MouseWheelEventPacket packet(event);

                int ret = _internalSocket->Send(&header, sizeof(header));
                if(ret != SOCKET_E_SUCCESS)
                    return ret;
                return _internalSocket->Send(&packet, sizeof(packet));
            }
            break;
        case MOUSE_EVENT_MOVE:
            {
                EventPacketHeader header(EVENT_PACKET_MM);
                MouseMoveEventPacket packet(event);

                int ret = _internalSocket->Send(&header, sizeof(header));
                if(ret != SOCKET_E_SUCCESS)
                    return ret;
                return _internalSocket->Send(&packet, sizeof(packet));
            }
            break;
        default:
            return SOCKET_E_INVALID_PARAM;
    }
}

int NetworkEntity::SendHIDEventPacket(const OSEvent& event)const
{
    // not implemented yet
    return SOCKET_E_UNKOWN;
}

NetworkEntity::NetworkEntity(Socket* socket) : _internalSocket(socket)
{}

int NetworkEntity::Send(const char* buff, const size_t size)const
{
    return _internalSocket->Send(buff, size);
}

int NetworkEntity::Send(const std::string toSend)const
{
    return _internalSocket->Send(toSend);
}

int NetworkEntity::SendOSEvent(const OSEvent& event)const
{
    switch(event.eventType)
    {
        case OS_EVENT_KEY:
            return SendKeyEventPacket(event);
            break;
        case OS_EVENT_MOUSE:
            return SendMouseEventPacket(event);
            break;
        case OS_EVENT_HID:
            return SendHIDEventPacket(event);
            break;
        default:
            return SOCKET_E_INVALID_PARAM;
    }
}

void NetworkEntity::AddDisplay(NativeDisplay* display, Rect bounds)
{
    displays.insert(DisplayEntry(display, bounds));
}

void NetworkEntity::RemoveDisplay(NativeDisplay* display)
{
    auto iter = displays.find(display);
    if(iter != displays.end())
    {
        displays.erase(iter);
        delete display;
    }
}

const NativeDisplay* NetworkEntity::PointIsInEntitiesDisplay(const Point& point)const
{
    // this might be able to be improved later but for now this is good enough
    for(auto& pair : displays)
    {
        // simple box collision algo
        if(pair.second.topLeft.x <= point.x && pair.second.topLeft.y <= point.y \
        && pair.second.bottomRight.x >= point.x && pair.second.bottomRight.y >= point.y)
        {
            return pair.first;
        }
    }

    return NULL;
}