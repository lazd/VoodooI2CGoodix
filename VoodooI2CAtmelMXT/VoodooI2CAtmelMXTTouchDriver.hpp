//
//  VoodooI2CAtmelMXTTouchDriver.hpp
//  VoodooI2CAtmelMXT
//
//  Created by CoolStar on 4/24/18.
//  Copyright © 2018 CoolStar. All rights reserved.
//

#ifndef VoodooI2CAtmelMXTTouchDriver_hpp
#define VoodooI2CAtmelMXTTouchDriver_hpp

#include <IOKit/IOService.h>
#include "../../../VoodooI2C/VoodooI2C/VoodooI2CDevice/VoodooI2CDeviceNub.hpp"
#include "../../../Multitouch Support/VoodooI2CMultitouchInterface.hpp"
#include "../../../Multitouch Support/MultitouchHelpers.hpp"
#include "../../../Dependencies/helpers.hpp"
#include "atmel_mxt.h"

#define __le16 UInt16
#define __le32 UInt32

class VoodooI2CAtmelMXTTouchDriver : public IOService {
    OSDeclareDefaultStructors(VoodooI2CAtmelMXTTouchDriver);
    
    VoodooI2CDeviceNub *api;
    IOACPIPlatformDevice *acpi_device;
public:
    /* Initialises the VoodooI2CAtmelMXTTouchDriver object/instance (intended as IOKit driver ctor)
     *
     * @return true if properly initialised
     */
    bool init(OSDictionary* properties) override;
    /* Frees any allocated resources, called implicitly by the kernel
     * as the last stage of the driver being unloaded
     *
     */
    void free() override;
    /* Checks if an Atmel device exists on the current system
     *
     * @return returns an instance of the current VoodooI2CAtmelMXTTouchDriver if there is a matched Atmel device, NULL otherwise
     */
    VoodooI2CAtmelMXTTouchDriver* probe(IOService* provider, SInt32* score) override;
    /* Starts the driver and initialises the Atmel device
     *
     * @return returns true if the driver has started
     */
    bool start(IOService* provider) override;
    /* Stops the driver and frees any allocated resource
     *
     */
    void stop(IOService* device) override;
    
protected:
    IOReturn setPowerState(unsigned long longpowerStateOrdinal, IOService* whatDevice) override;

private:
    bool awake;
    bool read_in_progress;
    bool ready_for_input;
    
    mxt_rollup core;
    int totsize = 0;
    
    mxt_object    *msgprocobj;
    mxt_object    *cmdprocobj;
    
    mxt_id_info info;
    
    UInt16 max_report_x;
    UInt16 max_report_y;
    
    uint8_t num_touchids;
    uint8_t multitouch;
    
    t7_config t7_cfg;
    
    uint8_t t100_aux_ampl;
    uint8_t t100_aux_area;
    uint8_t t100_aux_vect;
    
    /* Cached parameters from object table */
    uint16_t T5_address;
    uint8_t T5_msg_size;
    uint8_t T6_reportid;
    uint16_t T6_address;
    uint16_t T7_address;
    uint8_t T9_reportid_min;
    uint8_t T9_reportid_max;
    uint8_t T19_reportid;
    uint16_t T44_address;
    uint8_t T100_reportid_min;
    uint8_t T100_reportid_max;
    
    uint8_t max_reportid;
    
    uint8_t last_message_count;
    
    UInt8 Tipswitch[MXT_MAX_FINGERS];
    
    IOCommandGate* command_gate;
    IOInterruptEventSource* interrupt_source;
    VoodooI2CMultitouchInterface *mt_interface;
    OSArray* transducers;
    IOWorkLoop* workLoop;

    /* Handles input in a threaded manner, then
     * calls parse_ELAN_report via the command gate for synchronisation
     *
     */
    void handle_input_threaded();
    /* Sends the appropriate ELAN protocol packets to
     * initialise the device into multitouch mode
     *
     * @return true if the device was initialised properly
     */
    bool init_device();
    /* Handles any interrupts that the ELAN device generates
     * by spawning a thread that is out of the inerrupt context
     *
     */
    void interrupt_occurred(OSObject* owner, IOInterruptEventSource* src, int intCount);
    
    IOReturn parse_ATML_report();
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
    
    size_t mxt_obj_size(const mxt_object *obj);
    size_t mxt_obj_instances(const mxt_object *obj);
    mxt_object *mxt_findobject(struct mxt_rollup *core, int type);
    
    IOReturn mxt_read_reg(UInt16 reg, UInt8 *rbuf, int len);
    IOReturn mxt_write_reg_buf(UInt16 reg, UInt8 *xbuf, int len);
    IOReturn mxt_write_reg(UInt16 reg, UInt8 val);
    IOReturn mxt_write_object_off(mxt_object *obj, int offset, UInt8 val);
    void atmel_reset_device();
    IOReturn mxt_set_t7_power_cfg(UInt8 sleep);
    IOReturn mxt_read_t9_resolution();
    IOReturn mxt_read_t100_config();
    
    IOReturn ProcessMessagesUntilInvalid();
    IOReturn ProcessMessage(UInt8 *message);
    int ReadAndProcessMessages(UInt8 count);
    IOReturn DeviceReadT44();
    IOReturn DeviceRead();
};

#endif /* VoodooI2CAtmelMXTTouchDriver_hpp */
