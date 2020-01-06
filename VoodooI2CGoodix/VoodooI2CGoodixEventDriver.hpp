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

#include <IOKit/hidevent/IOHIDEventService.h>
#include <IOKit/hidsystem/IOHIDTypes.h>

#include "../../../Multitouch Support/VoodooI2CDigitiserStylus.hpp"
#include "../../../Multitouch Support/VoodooI2CMultitouchInterface.hpp"
#include "../../../Multitouch Support/MultitouchHelpers.hpp"

#include "../../../Dependencies/helpers.hpp"
#include "goodix.h"

/* Implements an HID Event Driver for HID devices that expose a digitiser usage page.
 *
 * The members of this class are responsible for parsing, processing and interpreting digitiser-related HID objects.
 */

class EXPORT VoodooI2CGoodixEventDriver : public IOHIDEventService {
  OSDeclareDefaultStructors(VoodooI2CGoodixEventDriver);

 public:
    /* Notification that a provider has been terminated, sent after recursing up the stack, in leaf-to-root order.
     * @options The terminated provider of this object.
     * @defer If there is pending I/O that requires this object to persist, and the provider is not opened by this object set defer to true and call the IOService::didTerminate() implementation when the I/O completes. Otherwise, leave defer set to its default value of false.
     *
     * @return *true*
     */

    bool didTerminate(IOService* provider, IOOptionBits options, bool* defer) override;

    const char* getProductName();

    /* Called during the start routine to set up the HID Event Driver
     * @provider The <IOHIDInterface> object which we have matched against.
     *
     * This function is reponsible for opening a client connection with the <IOHIDInterface> provider and for publishing
     * a multitouch interface into the IOService plane.
     *
     * @return *true* on successful start, *false* otherwise
     */

    bool handleStart(IOService* provider) override;

    /* Publishes a <VoodooI2CMultitouchInterface> into the IOService plane
     *
     * @return *kIOReturnSuccess* on successful publish, *kIOReturnError* otherwise.
     */

    IOReturn publishMultitouchInterface();
    void unpublishMultitouchInterface();

    /* Publishes some miscellaneous properties to the IOService plane
     */

    void setDigitizerProperties();

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
     */

    bool start(IOService* provider) override;

    void reportTouches(struct Touch touches[], int numTouches);

 protected:
    const char* name;
    bool awake = true;
//    IOHIDInterface* hid_interface;
    VoodooI2CMultitouchInterface* multitouch_interface;
    OSArray* transducers;

 private:
    OSSet* attached_hid_pointer_devices;
};


#endif /* VoodooI2CGoodixEventDriver_hpp */

