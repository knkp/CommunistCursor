#include "NativeInterface.h"
#include <iostream>
#include <thread>

#define SLEEPM(a) std::this_thread::sleep_for(std::chrono::milliseconds(a));

OSInterface OSInterface::sharedInterface;

OSInterface& OSInterface::SharedInterface()
{
    return sharedInterface;
}

OSInterface::OSInterface() :shouldRunMainloop(true), hasHookedEvents(false)
{
    static bool onceToken = true;
    if(onceToken)
    {
        onceToken = false;
        StoreScreenSize();
    }
}

OSInterfaceError OSInterface::ConvertEventToNativeCoords(const OSEvent inEvent, OSEvent& outEvent)
{
    int OSError = ::ConvertEventCoordsToNative(inEvent, outEvent);
    if(OSError != 0)
        return OSErrorToOSInterfaceError(OSError);
    return OS_E_SUCCESS;
}

OSInterfaceError OSInterface::SendMouseEvent(OSEvent mouseEvent)
{
    if(mouseEvent.eventType != OS_EVENT_MOUSE)
        return OS_E_INVALID_PARAM;

    int OSError = ::SendMouseEvent(mouseEvent);

    if(OSError == 0)
        return OS_E_SUCCESS;
    else
        return OSErrorToOSInterfaceError(OSError);
}

OSInterfaceError OSInterface::SendKeyEvent(OSEvent keyEvent)
{
    if(keyEvent.eventType != OS_EVENT_KEY)
        return OS_E_INVALID_PARAM;

    int OSError = ::SendKeyEvent(keyEvent);

    if(OSError == 0)
        return OS_E_SUCCESS;
    else
        return OSErrorToOSInterfaceError(OSError);
}

OSInterfaceError OSInterface::RegisterForOSEvents(OSEventCallback callback, void* userInfo)
{
    if(hasHookedEvents == false)
    {
        int res = NativeRegisterForOSEvents(this);
        if(res != 0)
        {
            std::cout << "Error hooking events with " << res << std::endl;
            return OSErrorToOSInterfaceError(res);
        }
            
        hasHookedEvents = true;
    }

    while(MapAccessMutex.try_lock() == false)SLEEPM(1);

    if(OSEventRegisterdCallbacks.count(userInfo) > 0)
    {
        MapAccessMutex.unlock();
        return OS_E_ALREADY_REGISTERED;
    }
    else
    {
        OSEventRegisterdCallbacks.insert(OSEventEntry(userInfo, callback));
        MapAccessMutex.unlock();
        return OS_E_SUCCESS;
    }
}

OSInterfaceError OSInterface::UnRegisterForOSEvents(void* userInfo)
{
    while(MapAccessMutex.try_lock() == false)SLEEPM(1);

    if(OSEventRegisterdCallbacks.count(userInfo) > 0)
    {
        OSEventRegisterdCallbacks.erase(userInfo);
        if(OSEventRegisterdCallbacks.empty())
        {
            NativeUnhookAllEvents();
            hasHookedEvents = false;
        }
        MapAccessMutex.unlock();
        return OS_E_SUCCESS;
    }
    else
    {
        MapAccessMutex.unlock();
        return OS_E_NOT_REGISTERED;
    }
}

void OSInterface::OSMainLoop()
{
    ::OSMainLoop(shouldRunMainloop);
}

void OSInterface::UpdateThread(OSEvent event)
{
    while(MapAccessMutex.try_lock() == false)SLEEPM(1);

    for(OSEventEntry entry : OSEventRegisterdCallbacks)
    {
        entry.second(event, entry.first);
    }

    MapAccessMutex.unlock();
}

std::ostream& operator<<(std::ostream& os, const OSEvent& event)
{
    switch(event.eventType)
    {
    case OS_EVENT_MOUSE:
        return os << "{" << "type:" << "Mouse Event" << " subType:" \
        << event.subEvent.raw << " mouseButton:" << event.eventButton.mouseButton\
        << " extendButtonInfo:" << event.extendButtonInfo << " pos {" << \
        event.posX << "," << event.posY << "}"; 
    case OS_EVENT_KEY:
        return os << "{" << "type:" << "Key Event" << " subType:" \
        << event.subEvent.raw << " scaneCode:" << event.eventButton.scanCode; 
    case OS_EVENT_HID:
        return os << "{" << "type:" << "HID Event" << " subType:" \
        << event.subEvent.raw << " button:" << event.eventButton.mouseButton\
        << " extendButtonInfo:" << event.extendButtonInfo << " pos {" << \
        event.posX << "," << event.posY << "}"; 
    case OS_EVENT_INVALID:
    default:
        return os << "Invalid Event";
    }
}

std::string OSEventTypeToString(OSEventType type)
{
    switch(type)
    {
    case OS_EVENT_MOUSE:
        break;
    case OS_EVENT_KEY:
        break;
    case OS_EVENT_HID:
        break;
    case OS_EVENT_INVALID:
        break;
    }
}
            
std::string MouseEventTypeToString(MouseEventType type)
{
    switch(type)
    {
    case MOUSE_EVENT_MOVE:
        break;
    case MOUSE_EVENT_DOWN:
        break;
    case MOUSE_EVENT_UP:
        break;
    case MOUSE_EVENT_SCROLL:
        break;
    case MOUSE_EVENT_INVALID:
        break;
    }
}
            
std::string KeyEventTypeToString(KeyEventType type)
{
    switch(type)
    {
    case KEY_EVENT_DOWN:
        break;
    case KEY_EVENT_UP:
        break;
    case KEY_EVENT_INVALID:
        break;
    }
}

std::string MouseButtonToString(MouseButton button)
{
    switch(button)
    {
    case MOUSE_BUTTON_LEFT:
        break;
    case MOUSE_BUTTON_RIGHT:
        break;
    case MOUSE_BUTTON_MIDDLE:
        break;
    case MOUSE_BUTTON_EXTENDED:
        break;
    case MOUSE_BUTTON_INVALID:
        break;
    }
}
