//
//  VoodooI2CGoodixEventDriver.hpp
//  VoodooI2CGoodix
//
//  Created by Larry Davis on 1/5/20.
//  Copyright © 2020 lazd. All rights reserved.
//

#ifndef VoodooI2CGoodixEventDriver_hpp
#define VoodooI2CGoodixEventDriver_hpp

// hack to prevent IOHIDEventDriver from loading when
// we include IOHIDEventService

#define _IOKIT_HID_IOHIDEVENTDRIVER_H

#include <libkern/OSBase.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>

#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDEventService.h>
#include <IOKit/hidsystem/IOHIDTypes.h>

#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IODisplay.h>

#include "../../../Multitouch Support/VoodooI2CDigitiserStylus.hpp"
#include "../../../Multitouch Support/VoodooI2CMultitouchInterface.hpp"
#include "../../../Multitouch Support/MultitouchHelpers.hpp"

#include "../../../Dependencies/helpers.hpp"

#define FINGER_LIFT_DELAY   50
#define CLICK_DELAY         100
#define RIGHT_CLICK_DELAY   500
#define HOVER       0x0
#define LEFT_CLICK  0x1
#define RIGHT_CLICK 0x2
#define DRAG        0x5
#define DOUBLE_CLICK_FAT_ZONE   40
#define DOUBLE_CLICK_TIME       450

//#define GOODIX_EVENT_DRIVER_CLICK_DEBUG
//#define GOODIX_EVENT_DRIVER_LIFT_DEBUG
//#define GOODIX_EVENT_DRIVER_HOVER_DEBUG
//#define GOODIX_EVENT_DRIVER_DRAG_DEBUG
//#define GOODIX_EVENT_DRIVER_DEBUG

struct Touch {
    int x;
    int y;
    int width;
    bool type; // 0 = finger, 1 = pen
};

/* Implements an HID Event Driver for HID devices that expose a digitiser usage page.
 *
 * The members of this class are responsible for parsing, processing and interpreting digitiser-related HID objects.
 */

class EXPORT VoodooI2CGoodixEventDriver : public IOHIDEventService {
  OSDeclareDefaultStructors(VoodooI2CGoodixEventDriver);

 public:
    /* Called during the start routine to set up the HID Event Driver
     * @provider The <IOHIDInterface> object which we have matched against.
     *
     * This function is reponsible for opening a client connection with the <IOHIDInterface> provider and for publishing
     * a multitouch interface into the IOService plane.
     *
     * @return *true* on successful start, *false* otherwise
     */

    bool handleStart(IOService* provider) override;

    /* Called by the OS in order to notify the driver that the device should change power state
     * @whichState The power state the device is expected to enter represented by either
     *  *kIOPMPowerOn* or *kIOPMPowerOff*
     * @whatDevice The power management policy maker
     *
     * This function exists to be overriden by inherited classes should they need it.
     *
     * @return *kIOPMAckImplied* on succesful state change, *kIOReturnError* otherwise
     */

    virtual IOReturn setPowerState(unsigned long whichState, IOService* whatDevice) override;

    /* Called during the stop routine to terminate the HID Event Driver
     * @provider The <IOHIDInterface> object which we have matched against.
     *
     * This function is reponsible for releasing the resources allocated in <start>
     */

    void handleStop(IOService* provider) override;

    /* Implemented to set a certain property
     * @provider The <IOHIDInterface> object which we have matched against.
     *
     * @return true if started successfully
     */

    bool start(IOService* provider) override;

    /* Report the passed touches as multitouch or digitizer events
     * @touches An array of Touch objects
     * @numTouches The number of touches in the Touch array
     */

    void reportTouches(struct Touch touches[], int numTouches, bool stylusButton1, bool stylusButton2);

    /* Initialize the multitouch interface with the provided logical size
     * @logicalMaxX The logical max X coordinate in pixels
     * @logicalMaxY The logical max Y coordinate in pixels
     * @numTransducers The maximum number of transducerrs
     * @vendorId The vendor ID of the touchscreen
     */

    void configureMultitouchInterface(int logicalMaxX, int logicalMaxY, int numTransducers, UInt32 vendorId);

 protected:
    VoodooI2CMultitouchInterface* multitouch_interface;
    OSArray* transducers;

    /* Publishes a <VoodooI2CMultitouchInterface> into the IOService plane
     *
     * @return *kIOReturnSuccess* on successful publish, *kIOReturnError* otherwise.
     */

    IOReturn publishMultitouchInterface();

    /* Unpublishes the <VoodooI2CMultitouchInterface> from the IOService plane
    */

    void unpublishMultitouchInterface();

    /* Publishes some miscellaneous properties to the IOService plane
     */

    void setDigitizerProperties();

    /* Dispatch a digitizer event at the given screen coordinate
     *
     * @logicalX The logical X position of the event
     * @logicalY The logical Y position of the event
     * @clickType what type of click to dispatch, if any
     */

    void dispatchDigitizerEvent(int logicalX, int logicalY, UInt32 clickType);

    /* Dispatch a pen event at the given screen coordinate
     *
     * @logicalX The logical X position of the event
     * @logicalY The logical Y position of the event
     * @pressure The pressure (0-1024)
     * @clickType what type of click to dispatch, if any
     */

    void dispatchPenEvent(int logicalX, int logicalY, int pressure, UInt32 clickType);

    /* Check if this interaction is within fat finger distance
     */
    bool isCloseToLastInteraction(UInt16 x, UInt16 y);

    /* Dispatch a finger lift event at the location of the last digitizer event
     */

    void fingerLift();

    /* Schedule a finger lift event
     */

    void scheduleLift();

    /* Get the active framebuffer
     */
    IOFramebuffer* getFramebuffer();

    /* Rotate coordinates to match current framebuffer's rotation
     *
     * @x A pointer to the X coordinate
     * @y A pointer to the Y coordinate
     */

    void checkRotation(IOFixed* x, IOFixed* y);

    /* Handle multitouch interactions
     *
     * @touches An array of Touch objects
     * @numTouches The number of touches
     */
    void handleMultitouchInteraction(struct Touch touches[], int numTouches);

    /* Handle singletouch interactions
     *
     * @touch A single Touch object
     */
    void handleSingletouchInteraction(Touch touch, bool stylusButton1, bool stylusButton2);

    /* Schedule a check for a click
     */

    void scheduleClickCheck();

    /* Dispatch a click if we're supposed to
     */
    void checkForClick();

private:
    IOWorkLoop *work_loop;
    IOTimerEventSource *liftTimerSource;
    IOTimerEventSource *clickTimerSource;
    IOFramebuffer* activeFramebuffer = NULL;

    UInt8 currentRotation;

    IOFixed lastEventFixedX = 0;
    IOFixed lastEventFixedY = 0;

    UInt16 nextLogicalX = 0;
    UInt16 nextLogicalY = 0;

    UInt16 lastClickX = 0;
    UInt16 lastClickY = 0;
    UInt64 lastClickTime = 0;

    UInt8 currentInteractionType = LEFT_CLICK;
    bool fingerDown = false;
    bool isMultitouch = false;
    bool movedDuringRightClick = false;
    UInt64 fingerDownStart = 0;

    UInt8 stylusTransducerID;

    bool scrollStarted = false;
};


#endif /* VoodooI2CGoodixEventDriver_hpp */

