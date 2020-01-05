//
//  VoodooI2CGoodixTouchDriver.hpp
//  VoodooI2CGoodix
//
//  Created by CoolStar on 4/24/18.
//  Copyright © 2018 CoolStar. All rights reserved.
//

#ifndef VoodooI2CGoodixTouchDriver_hpp
#define VoodooI2CGoodixTouchDriver_hpp

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include "../../../VoodooI2C/VoodooI2C/VoodooI2CDevice/VoodooI2CDeviceNub.hpp"
#include "../../../Multitouch Support/VoodooI2CMultitouchInterface.hpp"
#include "../../../Multitouch Support/MultitouchHelpers.hpp"
#include "../../../Dependencies/helpers.hpp"
#include "goodix.h"

class VoodooI2CGoodixTouchDriver : public IOService {
    OSDeclareDefaultStructors(VoodooI2CGoodixTouchDriver);

    VoodooI2CDeviceNub *api;
    IOACPIPlatformDevice *acpi_device;
public:
    /* Initialises the VoodooI2CGoodixTouchDriver object/instance (intended as IOKit driver ctor)
     *
     * @return true if properly initialised
     */
    bool init(OSDictionary* properties = 0) override;
    /* Frees any allocated resources, called implicitly by the kernel
     * as the last stage of the driver being unloaded
     *
     */
    void free() override;
    /* Checks if an Goodix device exists on the current system
     *
     * @return returns an instance of the current VoodooI2CGoodixTouchDriver if there is a matched Goodix device, NULL otherwise
     */
    VoodooI2CGoodixTouchDriver* probe(IOService* provider, SInt32* score) override;
    /* Starts the driver and initialises the Goodix device
     *
     * @return returns true if the driver has started
     */
    bool start(IOService* provider) override;
    /* Stops the driver and frees any allocated resource
     *
     */
    void stop(IOService* provider) override;
    
protected:
    IOReturn setPowerState(unsigned long powerState, IOService* whatDevice) override;

private:
    bool awake;
    bool read_in_progress;
    bool ready_for_input;

    struct goodix_ts_data *ts;

    IOCommandGate* command_gate;
    IOInterruptEventSource* interrupt_source;
    VoodooI2CMultitouchInterface *mt_interface;
    OSArray* transducers;
    IOWorkLoop* workLoop;

    /* Sends the appropriate packets to
     * initialise the device into multitouch mode
     *
     * @return true if the device was initialised properly
     */
    bool init_device();

    /* Initialises the VoodooI2C multitouch classes
     *
     * @return true if the VoodooI2C multitouch classes were properly initialised
     */
    bool publish_multitouch_interface();

    /* Releases any allocated resources (called by stop)
     *
     */
    void release_resources();

    /* Releases any allocated VoodooI2C multitouch device
     *
     */
    void unpublish_multitouch_interface();

    IOReturn goodix_read_reg(UInt16 reg, UInt8* values, size_t len);
    IOReturn goodix_write_reg(UInt16 reg, UInt8 value);

    /* Reads goodix touchscreen version
     */
    IOReturn goodix_read_version();

    /* Finish device initialization
     * Must be called from probe to finish initialization of the device.
     * Contains the common initialization code for both devices that
     * declare gpio pins and devices that do not. It is either called
     * directly from probe or from request_firmware_wait callback.
    */
    IOReturn goodix_configure_dev();

    /* Read the embedded configuration of the panel
     * Must be called during probe
     */
    void goodix_read_config();

    /* Handles any interrupts that the Goodix device generates
     * by spawning a thread that is out of the interrupt context
     */
    void interrupt_occurred(OSObject* owner, IOInterruptEventSource* src, int intCount);

    /* Handles input in a threaded manner, then
     * calls parse_goodix_report via the command gate for synchronisation
     */
    void handle_input_threaded();

    /* Process incoming events. Called when the IRQ is triggered.
     * Read the current device state, and push the input events to the user space.
     */
    IOReturn goodix_process_events();

    void goodix_ts_report_touch(UInt8 *coor_data, AbsoluteTime timestamp);

    int goodix_ts_read_input_report(UInt8 *data);
};

#endif /* VoodooI2CGoodixTouchDriver_hpp */
