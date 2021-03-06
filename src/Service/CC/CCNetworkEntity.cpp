#include "CCNetworkEntity.h"

#include <algorithm>

#include "../Socket/Socket.h"
#include "../OSInterface/OSTypes.h"
#include "../OSInterface/PacketTypes.h"
#include "../OSInterface/OSInterface.h"

#include "CCPacketTypes.h"
#include "CCDisplay.h"
#include "CCLogger.h"

#include "INetworkEntityDelegate.h"
#include "CCConfigurationManager.h"

using nlohmann::json;

void to_json(json& j, const Point& p) 
{
    j = json{ {"x", p.x}, {"y", p.y} };
}

void from_json(const json& j, Point& p) 
{
    j.at("x").get_to(p.x);
    j.at("y").get_to(p.y);
}

void to_json(json& j, const Rect& p) 
{
    j = json{ {"topLeft", p.topLeft}, {"bottomRight", p.bottomRight} };
}

void from_json(const json& j, Rect& p) 
{
    j.at("topLeft").get_to(p.topLeft);
    j.at("bottomRight").get_to(p.bottomRight);
}

int CCNetworkEntity::_jumpBuffer = 20;

SocketError CCNetworkEntity::SendRPCOfType(TCPPacketType rpcType, void* data, size_t dataSize)
{
    std::lock_guard<std::mutex> lock(_tcpMutex);

    if (_isLocalEntity)
        return SocketError::SOCKET_E_UNKOWN;

    NETCPPacketHeader packet((unsigned char)rpcType);

    SocketError error = _tcpCommSocket->Send(&packet, sizeof(packet));
    if (error != SocketError::SOCKET_E_SUCCESS)
    {
        if (ShouldRetryRPC(error))
        {
            return SendRPCOfType(rpcType, data, dataSize);
        }
        else return error;
    }

    error = WaitForAwk(_tcpCommSocket.get());
    if (error != SocketError::SOCKET_E_SUCCESS)
    {
        if (ShouldRetryRPC(error))
        {
            return SendRPCOfType(rpcType, data, dataSize);
        }
        else return error;
    }

    if (data && dataSize != 0)
    {
        error = _tcpCommSocket->Send(data, dataSize);
        if (error != SocketError::SOCKET_E_SUCCESS)
        {
            if (ShouldRetryRPC(error))
            {
                return SendRPCOfType(rpcType, data, dataSize);
            }
            else return error;
        }

        error = WaitForAwk(_tcpCommSocket.get());
        if (error != SocketError::SOCKET_E_SUCCESS)
        {
            if (ShouldRetryRPC(error))
            {
                return SendRPCOfType(rpcType, data, dataSize);
            }
            else return error;
        }
    }

    return SocketError::SOCKET_E_SUCCESS;
}

bool CCNetworkEntity::ShouldRetryRPC(SocketError error) const
{
    if (error == SocketError::SOCKET_E_NOT_CONNECTED)
    {
        SocketError error = _tcpCommSocket->Connect(); 
        if (error != SocketError::SOCKET_E_SUCCESS)
        {
            return false; 
        }
        return true;
    }
    else if (error == SocketError::SOCKET_E_BROKEN_PIPE)\
    {
        _tcpCommSocket->Close(true); 
        return true; 
    }
    else return false;
}

SocketError CCNetworkEntity::SendAwk(Socket* socket)
{
    std::lock_guard<std::mutex> lock(_tcpMutex);

    NETCPPacketAwk awk;
    return socket->Send(&awk, sizeof(awk));
}

SocketError CCNetworkEntity::WaitForAwk(Socket* socket)
{
    NETCPPacketAwk awk;
    size_t received;
    SocketError error = _tcpCommSocket->Recv((char*)&awk, sizeof(awk), &received);

    if (received != sizeof(awk) || awk.MagicNumber != P_MAGIC_NUMBER)
    {
        LOG_ERROR << "Error Receiving Awk, Invalid Packet received" << std::endl;
        return SocketError::SOCKET_E_INVALID_PACKET;
    }

    return error;
}

CCNetworkEntity::CCNetworkEntity(std::string entityID) : _entityID(entityID), _isLocalEntity(true), \
_shouldBeRunningCommThread(true), _delegate(0)
{
    // this is local so we make the server here
    int port = 1045; // this should be configured somehow at some point

    _tcpCommSocket = std::make_unique<Socket>(SOCKET_ANY_ADDRESS, port, false, SocketProtocol::SOCKET_P_TCP);
    _tcpCommThread = std::thread(&CCNetworkEntity::TCPCommThread, this);
}

CCNetworkEntity::CCNetworkEntity(std::string entityID, Socket* socket) : _entityID(entityID), _udpCommSocket(socket),\
_isLocalEntity(false), _shouldBeRunningCommThread(true), _delegate(0)
{
    // this is a remote entity so we create a tcp client here
    std::string address = socket->GetAddress();
    int port = 1045; // this should be configured somehow at some point

    // this is our comm socket, we don't need to do anything else at this point with it
    _tcpCommSocket = std::make_unique<Socket>(address, port, false, SocketProtocol::SOCKET_P_TCP);
    _tcpCommThread = std::thread(&CCNetworkEntity::HeartbeatThread, this);
}

CCNetworkEntity::~CCNetworkEntity()
{
    ShutdownThreads();
}

SocketError CCNetworkEntity::SendOSEvent(const OSEvent& event)
{
    if (_isLocalEntity)
    {
        LOG_ERROR << "Trying to send Event to Local Entity !" << std::endl;
        return SocketError::SOCKET_E_SUCCESS;
    }

    std::lock_guard<std::mutex> lock(_tcpMutex);

    if (_tcpCommSocket->GetIsConnected() == false)
    {
        SocketError error = _tcpCommSocket->Connect();
        if(error != SocketError::SOCKET_E_SUCCESS)
            return error;
    }

    NETCPPacketHeader header((char)TCPPacketType::OSEventHeader);
    SocketError ret = _tcpCommSocket->Send(&header, sizeof(header));
    if (ret != SocketError::SOCKET_E_SUCCESS)
        return ret;
    ret = WaitForAwk(_tcpCommSocket.get());
    if (ret != SocketError::SOCKET_E_SUCCESS)
        return ret;

    OSInputEventPacket packet(event);

    ret = _tcpCommSocket->Send(&packet, sizeof(packet));
    if (ret != SocketError::SOCKET_E_SUCCESS)
        return ret;

    return WaitForAwk(_tcpCommSocket.get());
}

SocketError CCNetworkEntity::ReceiveOSEvent(Socket* socket, OSEvent& newEvent)
{
    OSInputEventPacket packet;
    size_t received = 0;
    SocketError error = socket->Recv((char*)&packet, sizeof(packet), &received);

    if (error != SocketError::SOCKET_E_SUCCESS || received != sizeof(packet))
    {
        LOG_ERROR << "Error Trying Receive OS Event Packet From Server !: " << SOCK_ERR_STR(socket, error) << std::endl;
        return error;
    }

    newEvent = packet.AsOSEvent();

    return error;
}

void CCNetworkEntity::AddDisplay(std::shared_ptr<CCDisplay> display)
{
    _displays.push_back(display);

    auto displayBounds = display->GetCollision();

    _totalBounds.topLeft.x      = std::min(displayBounds.topLeft.x, _totalBounds.topLeft.x);
    _totalBounds.topLeft.y      = std::min(displayBounds.topLeft.y, _totalBounds.topLeft.y);
    _totalBounds.bottomRight.x  = std::max(displayBounds.bottomRight.x, _totalBounds.bottomRight.x);
    _totalBounds.bottomRight.y  = std::max(displayBounds.bottomRight.y, _totalBounds.bottomRight.y);
}

void CCNetworkEntity::RemoveDisplay(std::shared_ptr<CCDisplay> display)
{
    auto iter = std::find(_displays.begin(), _displays.end(),display);
    if(iter != _displays.end())
    {
        _displays.erase(iter);
    }
}

void CCNetworkEntity::SetDisplayOffsets(Point offsets)
{
    _offsets = offsets;

    _totalBounds.topLeft.x      =  400000;
    _totalBounds.topLeft.y      =  400000;
    _totalBounds.bottomRight.x  = -400000;
    _totalBounds.bottomRight.y  = -400000;

    for (auto display : _displays)
    {
        display->SetOffsets(_offsets.x, _offsets.y);

        auto displayBounds = display->GetCollision();

        _totalBounds.topLeft.x = std::min(displayBounds.topLeft.x, _totalBounds.topLeft.x);
        _totalBounds.topLeft.y = std::min(displayBounds.topLeft.y, _totalBounds.topLeft.y);
        _totalBounds.bottomRight.x = std::max(displayBounds.bottomRight.x, _totalBounds.bottomRight.x);
        _totalBounds.bottomRight.y = std::max(displayBounds.bottomRight.y, _totalBounds.bottomRight.y);
    }
}

const std::shared_ptr<CCDisplay> CCNetworkEntity::DisplayForPoint(const Point& point)const
{
    for(auto display : _displays)
    {
        if(display->PointIsInBounds(point))
            return display;
    }

    return NULL;
}

bool CCNetworkEntity::PointIntersectsEntity(const Point& p) const
{
    for (auto display : _displays)
    {
        if (display->PointIsInBounds(p))
            return true;
    }

    return false;
}

void CCNetworkEntity::AddEntityIfInProximity(CCNetworkEntity* entity)
{
    Rect collision = entity->_totalBounds;

    collision.topLeft = collision.topLeft + _offsets;
    collision.bottomRight = collision.bottomRight + _offsets;

    // create test collision Rects, possibly cache these

    Rect top = { {collision.topLeft.x, collision.topLeft.y + _jumpBuffer}, {collision.bottomRight.x, collision.topLeft.y} };
    Rect bottom = { {collision.topLeft.x, collision.bottomRight.y}, {collision.bottomRight.x, collision.bottomRight.y - _jumpBuffer} };
    Rect left   = { {collision.topLeft.x - _jumpBuffer, collision.topLeft.y}, {collision.topLeft.x, collision.bottomRight.y} };
    Rect right  = { {collision.bottomRight.x, collision.topLeft.y}, {collision.bottomRight.x + _jumpBuffer, collision.bottomRight.y} };

    if (collision.IntersectsRect(top))
    {
        _topEntites.push_back(entity);
        entity->_bottomEntites.push_back(this);
    }
    if (collision.IntersectsRect(bottom))
    {
        _bottomEntites.push_back(entity);
        entity->_topEntites.push_back(this);
    }
    if (collision.IntersectsRect(left))
    {
        _leftEntites.push_back(entity);
        entity->_rightEntites.push_back(this);
    }
    if (collision.IntersectsRect(right))
    {
        _rightEntites.push_back(entity);
        entity->_leftEntites.push_back(this);
    }
}

void CCNetworkEntity::ClearAllEntities()
{
    _leftEntites.clear();
    _topEntites.clear();
    _bottomEntites.clear();
    _rightEntites.clear();
}

void CCNetworkEntity::LoadFrom(const CCConfigurationManager& manager)
{
    manager.GetValue({ "Entities", _entityID }, _offsets);
    SetDisplayOffsets(_offsets);
}

void CCNetworkEntity::SaveTo(CCConfigurationManager& manager) const
{
    manager.SetValue({ "Entities", _entityID }, _offsets);
}

void CCNetworkEntity::ShutdownThreads()
{
    _shouldBeRunningCommThread = false;

    if (_udpCommSocket.get())
        _udpCommSocket->Close();

    if (_tcpCommSocket.get())
        _tcpCommSocket->Close();

    _tcpCommThread.join();
}

void CCNetworkEntity::RPC_SetMousePosition(float xPercent, float yPercent)
{
    if (_isLocalEntity)
    {
        // warp mouse
        int x = _totalBounds.topLeft.x + (int)((_totalBounds.bottomRight.x - _totalBounds.topLeft.x) * xPercent);
        int y = _totalBounds.topLeft.y + (int)((_totalBounds.bottomRight.y - _totalBounds.topLeft.y) * yPercent);
        LOG_ERROR << "RPC_SetMousePosition {" << x << "," << y << "}" << std::endl;

        // spawn thread for to send input on because we don't want a dead lock with messages
        // this is a windows issue and could be solved in OSInterface probably but for now
        // this will work

        std::thread thread([x,y]() {
            OSInterface::SharedInterface().SetMousePosition(x, y);
        });

        thread.detach();
    }
    else
    {
        NERPCSetMouseData data(xPercent, yPercent);
        SocketError error = SendRPCOfType(TCPPacketType::RPC_SetMousePosition, &data, sizeof(data));
        if (error != SocketError::SOCKET_E_SUCCESS)
        {
            LOG_ERROR << "Could not perform RPC_StartWarpingMouse!: " << SOCK_ERR_STR(_tcpCommSocket.get(), error) << std::endl;
        }
    }
}

void CCNetworkEntity::RPC_HideMouse()
{
    if (_isLocalEntity)
    {
        // hide mouse
        OSInterface::SharedInterface().SetMouseHidden(true);
    }
    else
    {
        SocketError error = SendRPCOfType(TCPPacketType::RPC_HideMouse);
        if (error != SocketError::SOCKET_E_SUCCESS)
        {
            LOG_ERROR << "Could not perform RPC_HideMouse!: " << SOCK_ERR_STR(_tcpCommSocket.get(), error) << std::endl;
        }
    }
}

void CCNetworkEntity::RPC_UnhideMouse()
{
    if (_isLocalEntity)
    {
        // stop hide mouse
        OSInterface::SharedInterface().SetMouseHidden(false);
    }
    else
    {
        SocketError error = SendRPCOfType(TCPPacketType::RPC_UnhideMouse);
        if (error != SocketError::SOCKET_E_SUCCESS)
        {
            LOG_ERROR << "Could not perform RPC!: " << SOCK_ERR_STR(_tcpCommSocket.get(), error) << std::endl;
        }
    }
}

void CCNetworkEntity::TCPCommThread()
{
    SocketError error = _tcpCommSocket->Bind();
    if (error != SocketError::SOCKET_E_SUCCESS)
    {
        LOG_ERROR << "Error Binding Network Entity TCP Comm Socket: " << SOCK_ERR_STR(_tcpCommSocket.get(), error) << std::endl;
    }

    error = _tcpCommSocket->Listen();
    if (error != SocketError::SOCKET_E_SUCCESS)
    {
        LOG_ERROR << "Error Listening From Network Entity TCP Comm Socket: " << SOCK_ERR_STR(_tcpCommSocket.get(), error) << std::endl;
    }

    while (_shouldBeRunningCommThread)
    {
        Socket* server = NULL;
        error = _tcpCommSocket->Accept(&server);
        if (error != SocketError::SOCKET_E_SUCCESS)
        {
            LOG_ERROR << "Error accepting server connection: " << SOCK_ERR_STR(_tcpCommSocket.get(), error) << std::endl;
            continue;
        }

        // we don't spawn a thread here because there should only ever be one server
        while (server->GetIsConnected() && _shouldBeRunningCommThread)
        {
            NETCPPacketHeader packet;
            size_t received = 0;
            error = server->Recv((char*)&packet, sizeof(packet), &received);
            if (error != SocketError::SOCKET_E_SUCCESS)
            {
                LOG_ERROR << "Error receiving NETCPPacketHeader from Server {" << server->GetAddress() << "} " << SOCK_ERR_STR(server, error) << std::endl;
                break;
            }
            
            if (received == sizeof(packet) && packet.MagicNumber == P_MAGIC_NUMBER)
            {
                SendAwk(server);

                LOG_INFO << "Received RPC ";
                switch ((TCPPacketType)packet.Type)
                {
                case TCPPacketType::RPC_SetMousePosition:
                    LOG_ERROR << "RPC_SetMousePosition" << std::endl;
                    {
                        NERPCSetMouseData data;
                        error = server->Recv((char*)&data, sizeof(data), &received);
                        if (error != SocketError::SOCKET_E_SUCCESS)
                        {
                            LOG_ERROR << "Error receiving NetworkEntityRPCSetMouseData Packet from Server {" << server->GetAddress() << "} " << SOCK_ERR_STR(server, error) << std::endl;
                            break;
                        }
                        if (received != sizeof(data))
                        {
                            LOG_ERROR << "Error Received invalid NetworkEntityRPCSetMouseData from Server {" << server->GetAddress() << "} " << SOCK_ERR_STR(server, error) << std::endl;
                            break;
                        }

                        SendAwk(server);

                        LOG_INFO << "NetworkEntityRPCSetMouseData {" << data.x << "," << data.y << "}" << std::endl;
                        RPC_SetMousePosition(data.x, data.y);
                    }
                    break;
                case TCPPacketType::RPC_HideMouse:
                    LOG_INFO << "RPC_HideMouse" << std::endl;
                    RPC_HideMouse();
                    break;
                case TCPPacketType::RPC_UnhideMouse:
                    LOG_INFO << "RPC_UnhideMouse" << std::endl;
                    RPC_UnhideMouse();
                    break;
                case TCPPacketType::Heartbeat:
                    LOG_INFO << "Received Heartbeat from server !" << std::endl;
                    break;
                case TCPPacketType::OSEventHeader:
                    LOG_INFO << "Received OS Event Header from server !" << std::endl;
                    {
                        OSEvent osEvent;

                        error = ReceiveOSEvent(server, osEvent);

                        if (error != SocketError::SOCKET_E_SUCCESS)
                        {
                            LOG_ERROR << "Error receiving OSEvent Data Packet from Server {" << server->GetAddress() << "} " << SOCK_ERR_STR(server, error) << std::endl;
                            break;
                        }

                        SendAwk(server);

                        LOG_INFO << osEvent << std::endl;

                        auto osError = OSInterface::SharedInterface().SendOSEvent(osEvent);
                        if (osError != OSInterfaceError::OS_E_SUCCESS)
                        {
                            LOG_ERROR << "Error Trying To Inject OS Event" << osEvent << " with error " << OSInterfaceErrorToString(osError) << std::endl;
                        }
                    }
                    break;
                }
            }
            else
            {
                LOG_ERROR << "Received Invalid TCP Packet from Server {" << server->GetAddress() << "} " << SOCK_ERR_STR(_tcpCommSocket.get(), error) << std::endl;
                break;
            }
            
        }
        delete server;
    
        if (_delegate)
            _delegate->LostServer();
    }
}

void CCNetworkEntity::HeartbeatThread()
{
    NETCPPacketHeader hearbeat((char)TCPPacketType::Heartbeat);

    if (_tcpCommSocket->GetIsConnected() == false)
    {
        std::lock_guard<std::mutex> lock(_tcpMutex);
        SocketError error = _tcpCommSocket->Connect();
        if (error != SocketError::SOCKET_E_SUCCESS)
        {
            if (_delegate)
            {
                _delegate->EntityLost(this);
                return;
            }
        }
    }

    while (_shouldBeRunningCommThread && _tcpCommSocket->GetIsConnected())
    {
        auto nextHeartbeat = std::chrono::system_clock::now() + std::chrono::seconds(5);
        {
            std::lock_guard<std::mutex> lock(_tcpMutex);
            SocketError error = _tcpCommSocket->Send((void*)&hearbeat, sizeof(hearbeat));

            if (error != SocketError::SOCKET_E_SUCCESS)
            {
                if (_delegate)
                {
                    _delegate->EntityLost(this);
                    return;
                }
            }

            NETCPPacketAwk awk;
            size_t received = 0;
            error = _tcpCommSocket->Recv((char*)&awk, sizeof(awk), &received);
            if (error != SocketError::SOCKET_E_SUCCESS)
            {
                if (_delegate)
                {
                    _delegate->EntityLost(this);
                    return;
                }
            }
            if (received != sizeof(awk) || awk.MagicNumber != P_MAGIC_NUMBER)
            {
                if (_delegate)
                {
                    _delegate->EntityLost(this);
                    return;
                }
            }
        }

        std::this_thread::sleep_until(nextHeartbeat);
    }
}

bool CCNetworkEntity::GetEntityForPointInJumpZone(Point& p, CCNetworkEntity** jumpEntity, JumpDirection& direction)const
{
    Rect collision = _totalBounds;

    collision.topLeft = collision.topLeft + _offsets;
    collision.bottomRight = collision.bottomRight + _offsets;

    if (p.y < (collision.topLeft.y + _jumpBuffer))
    {
        for (auto entity : _topEntites)
        {
            if (entity->PointIntersectsEntity({ p.x, p.y - _jumpBuffer }))
            {
                p.y -= _jumpBuffer;
                *jumpEntity = entity;
                direction = JumpDirection::UP;
                return true;
            }
        }
    }
    if (p.y > (collision.bottomRight.y - _jumpBuffer))
    {
        for (auto entity : _bottomEntites)
        {
            if (entity->PointIntersectsEntity({ p.x, p.y + _jumpBuffer }))
            {
                p.y += _jumpBuffer;
                *jumpEntity = entity;
                direction = JumpDirection::DOWN;
                return true;
            }
        }
    }
    if (p.x < (collision.topLeft.x + _jumpBuffer))
    {
        for (auto entity : _leftEntites)
        {
            if (entity->PointIntersectsEntity({ p.x - _jumpBuffer, p.y }))
            {
                p.x -= _jumpBuffer;
                *jumpEntity = entity;
                direction = JumpDirection::LEFT;
                return true;
            }
        }
    }
    if (p.x > (collision.bottomRight.x - _jumpBuffer))
    {
        for (auto entity : _rightEntites)
        {
            if (entity->PointIntersectsEntity({ p.x + _jumpBuffer, p.y }))
            {
                p.y = +_jumpBuffer;
                *jumpEntity = entity;
                direction = JumpDirection::RIGHT;
                return true;
            }
        }
    }

    // no jump zone
    return nullptr;
}
