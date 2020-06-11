#ifndef MOUSE_INTERFACE_H
#define MOUSE_INTERFACE_H

#include <functional>
#include <mutex>
#include <vector>

#include "OSInterfaceError.h"
#include "OSTypes.h"

class IOSEventReceiver;
class OSInterface
{
private:
    bool hasHookedEvents;
    bool shouldRunMainloop;
    std::mutex mapAccessMutex;
    std::vector<IOSEventReceiver*> eventReceivers;

    OSInterface();

    static OSInterface sharedInterface;
public:
    
    static OSInterface& SharedInterface();

    /*
     * Converts coordinates in {inEvent} to be understood by the OS and initializes {outEvent}
     * with those converted coords
     * This is really only needed for absolute coordinates in windows because they have a
     * weird normalizing system for mouse events
     */
    OSInterfaceError ConvertEventToNativeCoords(const OSEvent inEvent, OSEvent& outEvent);
    /*
     * Injects {mouseEvent} into OS as if it was given by a USB mouse
     */
    OSInterfaceError SendMouseEvent(const OSEvent mouseEvent);
    /*
     * injects {keyEvent} to OS as if it was given by a native hardware device
     */
    OSInterfaceError SendKeyEvent(const OSEvent keyEvent);
    /*
     * Registers {userInfo} to native event callbacks such as keyboard and mouse events.
     * {callback} is called when an event is received and {userInfo} is passed to that function
     */
    OSInterfaceError RegisterForOSEvents(IOSEventReceiver* newReceiver);
    /*
     * Removes {userInfo} from event callbacks
     */
    OSInterfaceError UnRegisterForOSEvents(IOSEventReceiver* toRemove);
    /*
     * Retreives a list of displays connected to the OS.
     * This only adds them to {displayList} and does not clear that list
     */
    OSInterfaceError GetNativeDisplayList(std::vector<NativeDisplay> &displayList);
    /*
     * Retreives all valid IP address that are not the  loop back from the OS.
     * The contents of outAddresses is filled with the address but it is not cleared
     * so new addresses are just appended to the list
     *
     *  returns a OSInterfaceError
     */
    OSInterfaceError GetIPAddressList(std::vector<IPAdressInfo>& outAddresses, const IPAdressInfoHints hints = {});
    /*
     * Gets the current mouse cursor position
     *
     * Returns an OSInterfaceError on failure or OS_E_SUCCESS if succesful
     */

    OSInterfaceError GetMousePosition(int& xPos, int& yPos);

    /*
     * OSMainLoop is a way to run the native event system of an OS. This should be called in the
     * same thread as RegisterForOSEvents, if event's want to be received by the user
     */
    void OSMainLoop();
    /*
     * Terminates main event loop of OS
     */
    void StopMainLoop();
    /*
     * Userd for Updating the event loop internally.
     * Return value determins if the event is consumed (not passed to OS)
     * True means it is and False means it lets the OS handle it
     *
     * Generally there is no need to call this
     */
    bool ConsumeInputEvent(OSEvent event);
};

extern std::string OSEventTypeToString(OSEventType);
extern std::string MouseEventTypeToString(MouseEventType);
extern std::string KeyEventTypeToString(KeyEventType);
extern std::string MouseButtonToString(MouseButton);

#endif
