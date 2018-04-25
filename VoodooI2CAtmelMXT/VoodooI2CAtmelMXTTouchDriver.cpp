//
//  VoodooI2CAtmelMXTTouchDriver.cpp
//  VoodooI2CAtmelMXT
//
//  Created by CoolStar on 4/24/18.
//  Copyright © 2018 CoolStar. All rights reserved.
//

#include "VoodooI2CAtmelMXTTouchDriver.hpp"
#include "atmel_mxt.h"

#define super IOService
OSDefineMetaClassAndStructors(VoodooI2CAtmelMXTTouchDriver, IOService);

void VoodooI2CAtmelMXTTouchDriver::free() {
    IOLog("%s:: VoodooI2CAtmel resources have been deallocated\n", getName());
    super::free();
}

void VoodooI2CAtmelMXTTouchDriver::handle_input_threaded() {
    if (!ready_for_input) {
        read_in_progress = false;
        return;
    }
    command_gate->attemptAction(OSMemberFunctionCast(IOCommandGate::Action, this, &VoodooI2CAtmelMXTTouchDriver::parse_ATML_report));
    read_in_progress = false;
}

bool VoodooI2CAtmelMXTTouchDriver::init(OSDictionary *properties) {
    transducers = NULL;
    if (!super::init(properties)) {
        return false;
    }
    transducers = OSArray::withCapacity(MXT_MAX_FINGERS);
    if (!transducers) {
        return false;
    }
    DigitiserTransducerType type = kDigitiserTransducerFinger;
    for (int i = 0; i < MXT_MAX_FINGERS; i++) {
        VoodooI2CDigitiserTransducer* transducer = VoodooI2CDigitiserTransducer::transducer(type, NULL);
        transducers->setObject(transducer);
    }
    awake = true;
    ready_for_input = false;
    read_in_progress = false;
    return true;
}

bool VoodooI2CAtmelMXTTouchDriver::init_device() {
    int blksize;
    uint32_t crc;
    IOReturn retVal = kIOReturnSuccess;
    
    retVal = mxt_read_reg(0, (UInt8 *)&core.info, sizeof(core.info));
    if (retVal != kIOReturnSuccess){
        IOLog("%s::Unable to read info\n", getName());
        return false;
    }
    
    core.nobjs = core.info.num_objects;
    
    if (core.nobjs < 0 || core.nobjs > 1024) {
        IOLog("%s::init_device nobjs (%d) out of bounds\n", getName(), core.nobjs);
        return false;
    }
    
    blksize = sizeof(core.info) +
    core.nobjs * sizeof(mxt_object);
    totsize = blksize + sizeof(mxt_raw_crc);
    
    core.buf = (uint8_t *)IOMalloc(totsize);
    
    retVal = mxt_read_reg(0, core.buf, totsize);
    if (retVal != kIOReturnSuccess){
        IOLog("%s::Unable to read buffer\n", getName());
        return false;
    }
    
    crc = obp_convert_crc((mxt_raw_crc *)((uint8_t *)core.buf + blksize));
    
    if (obp_crc24(core.buf, blksize) != crc) {
        IOLog("%s::init_device: configuration space "
              "crc mismatch %08x/%08x\n", getName(),
              crc, obp_crc24(core.buf, blksize));
    }
    else {
        IOLog("%s::init_device: CRC Matched!\n", getName());
    }
    
    core.objs = (mxt_object *)((uint8_t *)core.buf +
                               sizeof(core.info));
    
    msgprocobj = mxt_findobject(&core, MXT_GEN_MESSAGEPROCESSOR);
    if (!msgprocobj){
        IOLog("%s::Unable to find message processor\n", getName());
        return false;
    }
    cmdprocobj = mxt_findobject(&core, MXT_GEN_COMMANDPROCESSOR);
    if (!cmdprocobj){
        IOLog("%s::Unable to find command processor\n", getName());
        return false;
    }
    
    int reportid = 1;
    for (int i = 0; i < core.nobjs; i++) {
        mxt_object *obj = &core.objs[i];
        uint8_t min_id, max_id;
        
        if (obj->num_report_ids) {
            min_id = reportid;
            reportid += obj->num_report_ids *
            mxt_obj_instances(obj);
            max_id = reportid - 1;
        }
        else {
            min_id = 0;
            max_id = 0;
        }
        
        switch (obj->type) {
            case MXT_GEN_MESSAGE_T5:
                if (info.family == 0x80 &&
                    info.version < 0x20) {
                    /*
                     * On mXT224 firmware versions prior to V2.0
                     * read and discard unused CRC byte otherwise
                     * DMA reads are misaligned.
                     */
                    T5_msg_size = mxt_obj_size(obj);
                }
                else {
                    /* CRC not enabled, so skip last byte */
                    T5_msg_size = mxt_obj_size(obj) - 1;
                }
                T5_address = obj->start_address;
                break;
            case MXT_GEN_COMMAND_T6:
                T6_reportid = min_id;
                T6_address = obj->start_address;
                break;
            case MXT_GEN_POWER_T7:
                T7_address = obj->start_address;
                break;
            case MXT_TOUCH_MULTI_T9:
                multitouch = MXT_TOUCH_MULTI_T9;
                T9_reportid_min = min_id;
                T9_reportid_max = max_id;
                num_touchids = obj->num_report_ids
                * mxt_obj_instances(obj);
                break;
            case MXT_SPT_MESSAGECOUNT_T44:
                T44_address = obj->start_address;
                break;
            case MXT_SPT_GPIOPWM_T19:
                T19_reportid = min_id;
                break;
            case MXT_TOUCH_MULTITOUCHSCREEN_T100:
                multitouch = MXT_TOUCH_MULTITOUCHSCREEN_T100;
                T100_reportid_min = min_id;
                T100_reportid_max = max_id;
                
                /* first two report IDs reserved */
                num_touchids = obj->num_report_ids - 2;
                break;
        }
    }
    
    max_reportid = reportid;
    
    if (multitouch == MXT_TOUCH_MULTI_T9)
        mxt_read_t9_resolution();
    else if (multitouch == MXT_TOUCH_MULTITOUCHSCREEN_T100)
        mxt_read_t100_config();
    
    atmel_reset_device();
    
    if (multitouch == MXT_TOUCH_MULTITOUCHSCREEN_T100){
        mxt_set_t7_power_cfg(MXT_POWER_CFG_RUN);
    } else {
        mxt_object *obj = mxt_findobject(&core, MXT_TOUCH_MULTI_T9);
        mxt_write_object_off(obj, MXT_T9_CTRL, 0x83);
    }
    
    if (mt_interface){
        mt_interface->physical_max_x = max_report_x;
        mt_interface->physical_max_y = max_report_y;
        mt_interface->logical_max_x = max_report_x;
        mt_interface->logical_max_y = max_report_y;
    }
    return true;
}

void VoodooI2CAtmelMXTTouchDriver::interrupt_occurred(OSObject* owner, IOInterruptEventSource* src, int intCount) {
    if (read_in_progress)
        return;
    if (!awake)
        return;
    read_in_progress = true;
    thread_t new_thread;
    kern_return_t ret = kernel_thread_start(OSMemberFunctionCast(thread_continue_t, this, &VoodooI2CAtmelMXTTouchDriver::handle_input_threaded), this, &new_thread);
    if (ret != KERN_SUCCESS) {
        read_in_progress = false;
        IOLog("%s::Thread error while attemping to get input report\n", getName());
    } else {
        thread_deallocate(new_thread);
    }
}

IOReturn VoodooI2CAtmelMXTTouchDriver::ProcessMessagesUntilInvalid() {
    int count, read;
    UInt8 tries = 2;
    
    count = max_reportid;
    do {
        read = ReadAndProcessMessages(count);
        if (read < count)
            return kIOReturnSuccess;
    } while (--tries);
    return kIOReturnIOError;
}

IOReturn VoodooI2CAtmelMXTTouchDriver::ProcessMessage(UInt8 *message) {
    if (!transducers) {
        return kIOReturnBadArgument;
    }
    
    uint8_t report_id = message[0];
    
    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);
    
    if (report_id == 0xff)
        return 0;
    
    if (report_id == T6_reportid) {
        uint8_t status = message[1];
        uint32_t crc = message[2] | (message[3] << 8) | (message[4] << 16);
    }
    else if (report_id >= T9_reportid_min && report_id <= T9_reportid_max) {
        uint8_t flags = message[1];
        
        int rawx = (message[2] << 4) | ((message[4] >> 4) & 0xf);
        int rawy = (message[3] << 4) | ((message[4] & 0xf));
        
        /* Handle 10/12 bit switching */
        if (max_report_x < 1024)
            rawx >>= 2;
        if (max_report_y < 1024)
            rawy >>= 2;
        
        VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer,  transducers->getObject(report_id));
        transducer->type = kDigitiserTransducerFinger;
        
        transducer->is_valid = true;
        
        if(mt_interface) {
            transducer->logical_max_x = mt_interface->logical_max_x;
            transducer->logical_max_y = mt_interface->logical_max_y;
        }
        
        UInt8 tipswitch = (flags & (MXT_T9_DETECT | MXT_T9_PRESS)) != 0;
        
        transducer->coordinates.x.update(rawx, timestamp);
        transducer->coordinates.y.update(rawy, timestamp);
        transducer->tip_switch.update(tipswitch, timestamp);
        
        Tipswitch[report_id] = tipswitch;
        
        transducer->id = report_id;
        transducer->secondary_id = report_id;
    }
    else if (report_id >= T100_reportid_min && report_id <= T100_reportid_max) {
        int reportid = report_id - T100_reportid_min - 2;
        
        uint8_t flags = message[1];
        
        int rawx = *((uint16_t *)&message[2]);
        int rawy = *((uint16_t *)&message[4]);
        
        VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer,  transducers->getObject(report_id));
        transducer->type = kDigitiserTransducerFinger;
        
        transducer->is_valid = true;
        
        if(mt_interface) {
            transducer->logical_max_x = mt_interface->logical_max_x;
            transducer->logical_max_y = mt_interface->logical_max_y;
        }
        
        UInt8 tipswitch = (flags & (MXT_T100_DETECT)) != 0;
        Tipswitch[report_id] = tipswitch;
        
        transducer->coordinates.x.update(rawx, timestamp);
        transducer->coordinates.y.update(rawy, timestamp);
        transducer->tip_switch.update(tipswitch, timestamp);
        transducer->id = report_id;
        transducer->secondary_id = report_id;
    }
    
    UInt8 numFingers = 0;
    for (int i = 0; i < MXT_MAX_FINGERS; i++){
        if (Tipswitch[report_id])
            numFingers++;
    }
    
    VoodooI2CMultitouchEvent event;
    event.contact_count = numFingers;
    event.transducers = transducers;
    // send the event into the multitouch interface
    if (mt_interface) {
        mt_interface->handleInterruptReport(event, timestamp);
    }
    return 1;
}

int VoodooI2CAtmelMXTTouchDriver::ReadAndProcessMessages(UInt8 count) {
    uint8_t num_valid = 0;
    int i, ret;
    if (count > max_reportid)
        return -1;
    
    int msg_buf_size = max_reportid * T5_msg_size;
    uint8_t *msg_buf = (uint8_t *)IOMalloc(msg_buf_size);
    
    for (int i = 0; i < max_reportid * T5_msg_size; i++) {
        msg_buf[i] = 0xff;
    }
    
    mxt_read_reg(T5_address, msg_buf, T5_msg_size * count);
    
    for (i = 0; i < count; i++) {
        ret = ProcessMessage(msg_buf + T5_msg_size * i);
        
        if (ret == 1)
            num_valid++;
    }
    
    IOFree(msg_buf, msg_buf_size);
    
    /* return number of messages read */
    return num_valid;
}

IOReturn VoodooI2CAtmelMXTTouchDriver::DeviceReadT44() {
    IOLog("%s::DeviceReadT44\n", getName());
    uint8_t count, num_left;
    IOReturn retVal = kIOReturnSuccess;
    
    int msg_buf_size = T5_msg_size + 1;
    uint8_t *msg_buf = (uint8_t *)IOMalloc(msg_buf_size);
    
    /* Read T44 and T5 together */
    retVal = mxt_read_reg(T44_address, msg_buf, T5_msg_size);
    if (retVal != kIOReturnSuccess){
        goto end;
    }
    
    count = msg_buf[0];
    
    if (count == 0)
        goto end;
    
    if (count > max_reportid) {
        count = max_reportid;
    }
    
    retVal = ProcessMessage(msg_buf + 1);
    if (retVal != kIOReturnSuccess) {
        goto end;
    }
    
    num_left = count - 1;
    
    if (num_left) {
        int ret = ReadAndProcessMessages(num_left);
        if (ret < 0){
            retVal = kIOReturnIOError;
            goto end;
        }
    }
    
end:
    IOFree(msg_buf, msg_buf_size);
    return retVal;
}

IOReturn VoodooI2CAtmelMXTTouchDriver::DeviceRead() {
    IOLog("%s::DeviceRead\n", getName());
    int total_handled, num_handled;
    uint8_t count = last_message_count;
    
    if (count < 1 || count > max_reportid)
        count = 1;
    
    /* include final invalid message */
    total_handled = ReadAndProcessMessages(count + 1);
    if (total_handled < 0)
        return kIOReturnIOError;
    else if (total_handled <= count)
        goto update_count;
    
    /* keep reading two msgs until one is invalid or reportid limit */
    do {
        num_handled = ReadAndProcessMessages(2);
        if (num_handled < 0)
            return kIOReturnIOError;
        
        total_handled += num_handled;
        
        if (num_handled < 2)
            break;
    } while (total_handled < num_touchids);
    
update_count:
    last_message_count = total_handled;
    
    return kIOReturnSuccess;
}

IOReturn VoodooI2CAtmelMXTTouchDriver::parse_ATML_report(){
    if (T44_address)
        return DeviceReadT44();
    else
        return DeviceRead();
}

VoodooI2CAtmelMXTTouchDriver* VoodooI2CAtmelMXTTouchDriver::probe(IOService* provider, SInt32* score) {
    IOLog("%s::Touch probe\n", getName());
    if (!super::probe(provider, score)) {
        return NULL;
    }
    acpi_device = OSDynamicCast(IOACPIPlatformDevice, provider->getProperty("acpi-device"));
    if (!acpi_device) {
        IOLog("%s::Could not get ACPI device\n", getName());
        return NULL;
    }
    api = OSDynamicCast(VoodooI2CDeviceNub, provider);
    if (!api) {
        IOLog("%s::Could not get VoodooI2C API instance\n", getName());
        return NULL;
    }
    return this;
}

bool VoodooI2CAtmelMXTTouchDriver::publish_multitouch_interface() {
    mt_interface = new VoodooI2CMultitouchInterface();
    if (!mt_interface) {
        IOLog("%s::No memory to allocate VoodooI2CMultitouchInterface instance\n", getName());
        goto multitouch_exit;
    }
    if (!mt_interface->init(NULL)) {
        IOLog("%s::Failed to init multitouch interface\n", getName());
        goto multitouch_exit;
    }
    if (!mt_interface->attach(this)) {
        IOLog("%s::Failed to attach multitouch interface\n", getName());
        goto multitouch_exit;
    }
    if (!mt_interface->start(this)) {
        IOLog("%s::Failed to start multitouch interface\n", getName());
        goto multitouch_exit;
    }
    // Assume we are a touchscreen for now
    mt_interface->setProperty(kIOHIDDisplayIntegratedKey, true);
    // 0x03EB is Atmel's Vendor Id
    mt_interface->setProperty(kIOHIDVendorIDKey, 0x03EB, 32);
    mt_interface->setProperty(kIOHIDProductIDKey, 0x8A03, 32);
    return true;
multitouch_exit:
    unpublish_multitouch_interface();
    return false;
}

void VoodooI2CAtmelMXTTouchDriver::release_resources() {
    if (command_gate) {
        workLoop->removeEventSource(command_gate);
        command_gate->release();
        command_gate = NULL;
    }
    if (interrupt_source) {
        interrupt_source->disable();
        workLoop->removeEventSource(interrupt_source);
        interrupt_source->release();
        interrupt_source = NULL;
    }
    if (workLoop) {
        workLoop->release();
        workLoop = NULL;
    }
    if (acpi_device) {
        acpi_device->release();
        acpi_device = NULL;
    }
    if (api) {
        if (api->isOpen(this)) {
            api->close(this);
        }
        api->release();
        api = NULL;
    }
    if (transducers) {
        for (int i = 0; i < transducers->getCount(); i++) {
            OSObject* object = transducers->getObject(i);
            if (object) {
                object->release();
            }
        }
        OSSafeReleaseNULL(transducers);
    }
    if (totsize > 0)
        IOFree(core.buf, totsize);
}

IOReturn VoodooI2CAtmelMXTTouchDriver::setPowerState(unsigned long longpowerStateOrdinal, IOService* whatDevice) {
    if (whatDevice != this)
        return kIOReturnInvalid;
    if (longpowerStateOrdinal == 0) {
        if (awake) {
            awake = false;
            for (;;) {
                if (!read_in_progress) {
                    break;
                }
                IOSleep(10);
            }
            IOLog("%s::Going to sleep\n", getName());
        }
    } else {
        if (!awake) {
            atmel_reset_device();
            awake = true;
            IOLog("%s::Woke up and reset device\n", getName());
        }
    }
    return kIOPMAckImplied;
}

bool VoodooI2CAtmelMXTTouchDriver::start(IOService* provider) {
    if (!super::start(provider)) {
        return false;
    }
    workLoop = this->getWorkLoop();
    if (!workLoop) {
        IOLog("%s::Could not get a IOWorkLoop instance\n", getName());
        return false;
    }
    workLoop->retain();
    command_gate = IOCommandGate::commandGate(this);
    if (!command_gate || (workLoop->addEventSource(command_gate) != kIOReturnSuccess)) {
        IOLog("%s::Could not open command gate\n", getName());
        goto start_exit;
    }
    acpi_device->retain();
    api->retain();
    if (!api->open(this)) {
        IOLog("%s::Could not open API\n", getName());
        goto start_exit;
    }
    // set interrupts AFTER device is initialised
    interrupt_source = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &VoodooI2CAtmelMXTTouchDriver::interrupt_occurred), api, 0);
    if (!interrupt_source) {
        IOLog("%s::Could not get interrupt event source\n", getName());
        goto start_exit;
    }
    publish_multitouch_interface();
    if (!init_device()) {
        IOLog("%s::Failed to init device\n", getName());
        return NULL;
    }
    workLoop->addEventSource(interrupt_source);
    interrupt_source->enable();
    PMinit();
    api->joinPMtree(this);
    registerPowerDriver(this, VoodooI2CIOPMPowerStates, kVoodooI2CIOPMNumberPowerStates);
    IOSleep(100);
    ready_for_input = true;
    setProperty("VoodooI2CServices Supported", OSBoolean::withBoolean(true));
    IOLog("%s::VoodooI2CAtmelMXT has started\n", getName());
    mt_interface->registerService();
    registerService();
    return true;
start_exit:
    release_resources();
    return false;
}

void VoodooI2CAtmelMXTTouchDriver::stop(IOService* provider) {
    release_resources();
    unpublish_multitouch_interface();
    PMstop();
    IOLog("%s::VoodooI2CAtmelMXT has stopped\n", getName());
    super::stop(provider);
}

void VoodooI2CAtmelMXTTouchDriver::unpublish_multitouch_interface() {
    if (mt_interface) {
        mt_interface->stop(this);
        mt_interface->release();
        mt_interface = NULL;
    }
}

size_t VoodooI2CAtmelMXTTouchDriver::mxt_obj_size(const mxt_object *obj)
{
    return obj->size_minus_one + 1;
}

size_t VoodooI2CAtmelMXTTouchDriver::mxt_obj_instances(const mxt_object *obj)
{
    return obj->instances_minus_one + 1;
}

mxt_object *VoodooI2CAtmelMXTTouchDriver::mxt_findobject(struct mxt_rollup *core, int type)
{
    int i;
    
    for (i = 0; i < core->nobjs; ++i) {
        if (core->objs[i].type == type)
            return(&core->objs[i]);
    }
    return NULL;
}

IOReturn VoodooI2CAtmelMXTTouchDriver::mxt_read_reg(UInt16 reg, UInt8 *rbuf, int len)
{
    UInt8 wreg[2];
    wreg[0] = reg & 255;
    wreg[1] = reg >> 8;
    
    IOReturn retVal = kIOReturnSuccess;
    retVal = api->writeReadI2C(reinterpret_cast<UInt8*>(wreg), sizeof(wreg), rbuf, len);
    return retVal;
}

IOReturn VoodooI2CAtmelMXTTouchDriver::mxt_write_reg_buf(UInt16 reg, UInt8 *xbuf, int len)
{
    UInt8 wreg[2];
    wreg[0] = reg & 255;
    wreg[1] = reg >> 8;
    
    UInt8 *intermbuf = (uint8_t *)IOMalloc(sizeof(wreg) + len);
    memcpy(intermbuf, wreg, sizeof(wreg));
    memcpy(intermbuf + sizeof(wreg), xbuf, len);
    
    IOReturn retVal = api->writeI2C(intermbuf, sizeof(wreg) + len);
    IOFree(intermbuf, sizeof(wreg) + len);
    return retVal;
}

IOReturn VoodooI2CAtmelMXTTouchDriver::mxt_write_reg(UInt16 reg, UInt8 val)
{
    return mxt_write_reg_buf(reg, &val, 1);
}

IOReturn
VoodooI2CAtmelMXTTouchDriver::mxt_write_object_off(mxt_object *obj,
                                                    int offset, UInt8 val)
{
    uint16_t reg = obj->start_address;
    
    reg += offset;
    return mxt_write_reg(reg, val);
}

void VoodooI2CAtmelMXTTouchDriver::atmel_reset_device()
{
    for (int i = 0; i < MXT_MAX_FINGERS; i++){
        Tipswitch[i] = 0;
    }

    mxt_write_object_off(cmdprocobj, MXT_CMDPROC_RESET_OFF, 1);
    IOLog("%s::Reset sent!\n", getName());
    IOSleep(100);
}

IOReturn VoodooI2CAtmelMXTTouchDriver::mxt_set_t7_power_cfg(UInt8 sleep)
{
    t7_config *new_config;
    t7_config deepsleep;
    deepsleep.active = deepsleep.idle = 0;
    t7_config active;
    active.active = 20;
    active.idle = 100;
    
    if (sleep == MXT_POWER_CFG_DEEPSLEEP)
        new_config = &deepsleep;
    else
        new_config = &active;
    return mxt_write_reg_buf(T7_address, (UInt8 *)new_config, sizeof(t7_cfg));
}

IOReturn VoodooI2CAtmelMXTTouchDriver::mxt_read_t9_resolution()
{
    IOReturn retVal = kIOReturnSuccess;
    
    t9_range range;
    unsigned char orient;
    
    mxt_object *resolutionobject = mxt_findobject(&core, MXT_TOUCH_MULTI_T9);
    
    if (!resolutionobject){
        IOLog("%s::Unable to find T9 object\n", getName());
        return kIOReturnInvalid;
    }
    
    retVal = mxt_read_reg(resolutionobject->start_address + MXT_T9_RANGE, (UInt8 *)&range, sizeof(range));
    if (retVal != kIOReturnSuccess){
        IOLog("%s::Unable to read T9 range\n", getName());
        return retVal;
    }
    
    retVal = mxt_read_reg(resolutionobject->start_address + MXT_T9_ORIENT, &orient, 1);
    if (retVal != kIOReturnSuccess){
        IOLog("%s::Unable to read T9 orientation\n", getName());
        return retVal;
    }
    
    /* Handle default values */
    if (range.x == 0)
        range.x = 1023;
    
    if (range.y == 0)
        range.y = 1023;
    
    if (orient & MXT_T9_ORIENT_SWITCH) {
        max_report_x = range.y + 1;
        max_report_y = range.x + 1;
    }
    else {
        max_report_x = range.x + 1;
        max_report_y = range.y + 1;
    }
    IOLog("%s:: Screen Size: X: %d Y: %d\n", getName(), max_report_x, max_report_y);
    return retVal;
}

IOReturn VoodooI2CAtmelMXTTouchDriver::mxt_read_t100_config()
{
    IOReturn retVal = kIOReturnSuccess;
    uint16_t range_x, range_y;
    uint8_t cfg, tchaux;
    uint8_t aux;
    
    mxt_object *resolutionobject = mxt_findobject(&core, MXT_TOUCH_MULTITOUCHSCREEN_T100);
    if (!resolutionobject){
        IOLog("%s::Unable to find T100 object\n", getName());
        return kIOReturnInvalid;
    }
    
    /* read touchscreen dimensions */
    retVal = mxt_read_reg(resolutionobject->start_address + MXT_T100_XRANGE, (UInt8 *)&range_x, sizeof(range_x));
    if (retVal != kIOReturnSuccess){
        IOLog("%s::Unable to read T100 xrange\n", getName());
        return retVal;
    }
    
    retVal = mxt_read_reg(resolutionobject->start_address + MXT_T100_YRANGE, (UInt8 *)&range_y, sizeof(range_y));
    if (retVal != kIOReturnSuccess){
        IOLog("%s::Unable to read T100 yrange\n", getName());
        return retVal;
    }
    
    /* read orientation config */
    retVal = mxt_read_reg(resolutionobject->start_address + MXT_T100_CFG1, &cfg, 1);
    if (retVal != kIOReturnSuccess){
        IOLog("%s::Unable to read T100 orientation config\n", getName());
        return retVal;
    }
    
    if (cfg & MXT_T100_CFG_SWITCHXY) {
        max_report_x = range_y + 1;
        max_report_y = range_x + 1;
    }
    else {
        max_report_x = range_x + 1;
        max_report_y = range_y + 1;
    }
    
    retVal = mxt_read_reg(resolutionobject->start_address + MXT_T100_TCHAUX, &tchaux, 1);
    if (retVal != kIOReturnSuccess){
        IOLog("%s::Unable to read T100 aux bits\n", getName());
        return retVal;
    }
    
    aux = 6;
    
    if (tchaux & MXT_T100_TCHAUX_VECT)
        t100_aux_vect = aux++;
    
    if (tchaux & MXT_T100_TCHAUX_AMPL)
        t100_aux_ampl = aux++;
    
    if (tchaux & MXT_T100_TCHAUX_AREA)
        t100_aux_area = aux++;
    IOLog("%s::Screen Size T100: X: %d Y: %d\n", getName(), max_report_x, max_report_y);
    return retVal;
}
