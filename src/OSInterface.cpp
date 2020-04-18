#include "OSInterface.h"
#include <iostream>

#define SLEEPM(a) std::this_thread::sleep_for(std::chrono::milliseconds(a));

extern int NativeRegisterForOSEvents(OSInterface* osi);
extern void OSMainLoop(bool& stopSwitch);
extern void NativeUnhookAllEvents();

OSInterface OSInterface::sharedInterface;

OSInterface& OSInterface::SharedInterface()
{
    return sharedInterface;
}

OSInterfaceError OSInterface::SetMousePosition(int x, int y)
{
    return OS_E_SUCCESS;
}

OSInterfaceError OSInterface::GetMousePosition(int *x, int *y)
{
    return OS_E_SUCCESS;
}

OSInterfaceError OSInterface::RegisterForOSEvents(OSEventCallback callback, void* userInfo)
{
    if(hasHookedEvents == false)
    {
        int res = NativeRegisterForOSEvents(this);
        if(res != 0)
        {
            std::cout << "Error hooking events with " << res << std::endl;
            return OS_E_NOT_REGISTERED;
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