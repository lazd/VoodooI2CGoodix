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
#include <IOKit/IOTimerEventSource.h>

#include <IOKit/hidevent/IOHIDEventService.h>
#include <IOKit/hidsystem/IOHIDTypes.h>

#include "../../../Multitouch Support/VoodooI2CDigitiserStylus.hpp"
#include "../../../Multitouch Support/VoodooI2CMultitouchInterface.hpp"
#include "../../../Multitouch Support/MultitouchHelpers.hpp"

#include "../../../Dependencies/helpers.hpp"

#define FINGER_LIFT_EVENT_DELAY 14

struct Touch {
    int x;
    int y;
    int width;
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

    void reportTouches(struct Touch touches[], int numTouches);

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
     * @logicalX The logical X position of the event
     * @logicalY The logical Y position of the event
     * @click Whether this is a click event
     */

    void dispatchDigitizerEvent(int logicalX, int logicalY, bool click);

    /* Dispatch a finger lift event at the location of the last digitizer event
     */

    void fingerLift();

    /* Schedule a finger lift event
     */
    void scheduleLift();

 private:
    IOWorkLoop *work_loop;
    IOTimerEventSource *timer_source;

    UInt32 buttons = 0;
    IOFixed last_x = 0;
    IOFixed last_y = 0;
    SInt32 last_id = 0;
};


#endif /* VoodooI2CGoodixEventDriver_hpp */

