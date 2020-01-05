//
//  VoodooI2CGoodixTouchDriver.cpp
//  VoodooI2CGoodix
//
//  Created by CoolStar on 4/24/18.
//  Copyright © 2018 CoolStar. All rights reserved.
//

#include "VoodooI2CGoodixTouchDriver.hpp"
#include "goodix.h"
#include <libkern/OSByteOrder.h>

#define super IOService
OSDefineMetaClassAndStructors(VoodooI2CGoodixTouchDriver, IOService);

struct goodix_chip_data {
    UInt16 config_addr;
    int config_len;
};

struct goodix_ts_data {
    const struct goodix_chip_data *chip;
    int abs_x_max;
    int abs_y_max;
    bool swapped_x_y;
    bool inverted_x;
    bool inverted_y;
    unsigned int max_touch_num;
    unsigned int int_trigger_type;
    UInt16 id;
    UInt16 version;
};

static const struct goodix_chip_data gt1x_chip_data = {
    .config_addr        = GOODIX_GT1X_REG_CONFIG_DATA,
    .config_len        = GOODIX_CONFIG_MAX_LENGTH
};

static const struct goodix_chip_data gt911_chip_data = {
    .config_addr        = GOODIX_GT9X_REG_CONFIG_DATA,
    .config_len        = GOODIX_CONFIG_911_LENGTH
};

static const struct goodix_chip_data gt967_chip_data = {
    .config_addr        = GOODIX_GT9X_REG_CONFIG_DATA,
    .config_len        = GOODIX_CONFIG_967_LENGTH
};

static const struct goodix_chip_data gt9x_chip_data = {
    .config_addr        = GOODIX_GT9X_REG_CONFIG_DATA,
    .config_len        = GOODIX_CONFIG_MAX_LENGTH
};

static const struct goodix_chip_data *goodix_get_chip_data(UInt16 id)
{
    switch (id) {
    case 1151:
        return &gt1x_chip_data;

    case 911:
    case 9271:
    case 9110:
    case 9111:
    case 927:
    case 928:
        return &gt911_chip_data;

    case 912:
    case 967:
        return &gt967_chip_data;

    default:
        return &gt9x_chip_data;
    }
};

/* Temp: Taken from the Linux kernel source */
static inline uint16_t __get_unaligned_le16(const uint8_t *p) {
    return p[0] | p[1] << 8;
}

static inline uint16_t get_unaligned_le16(const void *p) {
    return __get_unaligned_le16((const uint8_t *)p);
}

/* This is supposed to be a sub for kstrtou16(), adapted from https://stackoverflow.com/a/20020795/1170723 */
static inline bool str_to_uint16(const char *str, uint16_t *res) {
    char *end;
    long val = strtol(str, &end, 10);
    if (end == str || *end != '\0' || val < 0 || val >= 0x10000) {
        return false;
    }
    *res = (uint16_t)val;
    return true;
}

static inline void swap(int& x, int& y) {
    int z = x;
    x = y;
    y = z;
}

bool VoodooI2CGoodixTouchDriver::init(OSDictionary *properties) {
    transducers = NULL;
    if (!super::init(properties)) {
        return false;
    }
    transducers = OSArray::withCapacity(GOODIX_MAX_CONTACTS);
    if (!transducers) {
        return false;
    }
    DigitiserTransducerType type = kDigitiserTransducerFinger;
    for (int i = 0; i < GOODIX_MAX_CONTACTS; i++) {
        VoodooI2CDigitiserTransducer* transducer = VoodooI2CDigitiserTransducer::transducer(type, NULL);
        transducers->setObject(transducer);
    }
    awake = true;
    ready_for_input = false;
    read_in_progress = false;
    return true;
}

void VoodooI2CGoodixTouchDriver::free() {
    IOLog("%s::Freeing\n", getName());
    super::free();
}

VoodooI2CGoodixTouchDriver* VoodooI2CGoodixTouchDriver::probe(IOService* provider, SInt32* score) {
    IOLog("%s::Probing\n", getName());
    if (!super::probe(provider, score)) {
        return NULL;
    }
    acpi_device = OSDynamicCast(IOACPIPlatformDevice, provider->getProperty("acpi-device"));
    if (!acpi_device) {
        IOLog("Could not get ACPI device\n");
        return NULL;
    }
    api = OSDynamicCast(VoodooI2CDeviceNub, provider);
    if (!api) {
        IOLog("Could not get VoodooI2C API instance\n");
        return NULL;
    }
    return this;
}

// Actual start sequence commented out for now
bool VoodooI2CGoodixTouchDriver::start(IOService* provider) {
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
    interrupt_source = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &VoodooI2CGoodixTouchDriver::interrupt_occurred), api, 0);
    if (!interrupt_source) {
        IOLog("%s::Could not get interrupt event source\n", getName());
        goto start_exit;
    }
    publish_multitouch_interface();
     if (!init_device()) {
        IOLog("%s::Failed to init device\n", getName());
        goto start_exit;
    }
    else {
        IOLog("%s::Device initialized\n", getName());
    }
    workLoop->addEventSource(interrupt_source);
    interrupt_source->enable();
    PMinit();
    api->joinPMtree(this);
    registerPowerDriver(this, VoodooI2CIOPMPowerStates, kVoodooI2CIOPMNumberPowerStates);
    IOSleep(100);
    ready_for_input = true;
    setProperty("VoodooI2CServices Supported", OSBoolean::withBoolean(true));
    IOLog("%s::VoodooI2CGoodixTouchDriver has started\n", getName());
    mt_interface->registerService();
    registerService();
    return true;
start_exit:
    release_resources();
    return false;
}

void VoodooI2CGoodixTouchDriver::interrupt_occurred(OSObject* owner, IOInterruptEventSource* src, int intCount) {
    if (read_in_progress)
        return;
    if (!awake)
        return;
    read_in_progress = true;
    thread_t new_thread;
    kern_return_t ret = kernel_thread_start(OSMemberFunctionCast(thread_continue_t, this, &VoodooI2CGoodixTouchDriver::handle_input_threaded), this, &new_thread);
    if (ret != KERN_SUCCESS) {
        read_in_progress = false;
        IOLog("%s::Thread error while attemping to get input report: %d\n", getName(), ret);
    } else {
        thread_deallocate(new_thread);
    }
}

void VoodooI2CGoodixTouchDriver::handle_input_threaded() {
    if (!ready_for_input) {
        read_in_progress = false;
        return;
    }
    command_gate->attemptAction(OSMemberFunctionCast(IOCommandGate::Action, this, &VoodooI2CGoodixTouchDriver::goodix_process_events));
    read_in_progress = false;
}

IOReturn VoodooI2CGoodixTouchDriver::goodix_process_events() {
    UInt8 point_data[1 + GOODIX_CONTACT_SIZE * GOODIX_MAX_CONTACTS];

    int touch_num;
    int i;

    touch_num = goodix_ts_read_input_report(point_data);
    if (touch_num < 0) {
        return kIOReturnSuccess;
    }

    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);

    /*
     * Bit 4 of the first byte reports the status of the capacitive
     * Windows/Home button.
     */
//    input_report_key(ts->input_dev, KEY_LEFTMETA, point_data[0] & BIT(4));

    for (i = 0; i < touch_num; i++) {
        goodix_ts_report_touch(&point_data[1 + GOODIX_CONTACT_SIZE * i], timestamp);
    }

    IOReturn retVal = goodix_write_reg(GOODIX_READ_COOR_ADDR, 0);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::I2C write end_cmd error: %d\n", getName(), retVal);
        return retVal;
    }

    if (touch_num > 0) {
        VoodooI2CMultitouchEvent event;
        event.contact_count = touch_num;
        event.transducers = transducers;
        // send the event into the multitouch interface
        if (mt_interface) {
            mt_interface->handleInterruptReport(event, timestamp);
        }
    }

    return kIOReturnSuccess;
}

int VoodooI2CGoodixTouchDriver::goodix_ts_read_input_report(UInt8 *data) {
    uint64_t max_timeout;
    int touch_num;
    IOReturn retVal;

    AbsoluteTime timestamp;
    uint64_t timestamp_ns;
    clock_get_uptime(&timestamp);
    absolutetime_to_nanoseconds(timestamp, &timestamp_ns);

    /*
     * The 'buffer status' bit, which indicates that the data is valid, is
     * not set as soon as the interrupt is raised, but slightly after.
     * This takes around 10 ms to happen, so we poll for GOODIX_BUFFER_STATUS_TIMEOUT (20ms).
     */
    max_timeout = timestamp_ns + GOODIX_BUFFER_STATUS_TIMEOUT;
    do {
        clock_get_uptime(&timestamp);
        absolutetime_to_nanoseconds(timestamp, &timestamp_ns);

        retVal = goodix_read_reg(GOODIX_READ_COOR_ADDR, data, GOODIX_CONTACT_SIZE + 1);
        if (retVal != kIOReturnSuccess) {
            IOLog("%s::I2C transfer starting coordinate read: %d\n", getName(), retVal);
            return -1;
        }
        if (data[0] & GOODIX_BUFFER_STATUS_READY) {
            touch_num = data[0] & 0x0f;
            if (touch_num > ts->max_touch_num) {
                IOLog("%s::Error: got more touches than we should have (got %d, max = %d)\n", getName(), touch_num, ts->max_touch_num);
                return -1;
            }

            if (touch_num > 1) {
                data += 1 + GOODIX_CONTACT_SIZE;
                retVal = goodix_read_reg(GOODIX_READ_COOR_ADDR + 1 + GOODIX_CONTACT_SIZE, data, GOODIX_CONTACT_SIZE * (touch_num - 1));
                if (retVal != kIOReturnSuccess) {
                    IOLog("%s::I2C transfer error during coordinate read: %d\n", getName(), retVal);
                    return -1;
                }
            }

            return touch_num;
        }

        usleep_range(1000, 2000); /* Poll every 1 - 2 ms */
    } while (timestamp_ns < max_timeout);

    /*
     * The Goodix panel will send spurious interrupts after a
     * 'finger up' event, which will always cause a timeout.
     */
    return 0;
}

void VoodooI2CGoodixTouchDriver::goodix_ts_report_touch(UInt8 *coor_data, AbsoluteTime timestamp) {
    int id = coor_data[0] & 0x0F;
    int input_x = get_unaligned_le16(&coor_data[1]);
    int input_y = get_unaligned_le16(&coor_data[3]);
    int input_w = get_unaligned_le16(&coor_data[5]);

    /* Inversions have to happen before axis swapping */
    if (ts->inverted_x)
        input_x = ts->abs_x_max - input_x;
    if (ts->inverted_y)
        input_y = ts->abs_y_max - input_y;
    if (ts->swapped_x_y)
        swap(input_x, input_y);

    IOLog("%s::Touch %d at %d, %d with width %d\n", getName(), id, input_x, input_y, input_w);

    if (!transducers) {
        IOLog("%s::No transducers, cannot report", getName());
        return;
    }

    VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer, transducers->getObject(id));
    transducer->type = kDigitiserTransducerFinger;

    transducer->is_valid = true;

    if (mt_interface) {
        transducer->logical_max_x = mt_interface->logical_max_x;
        transducer->logical_max_y = mt_interface->logical_max_y;
    }

    transducer->coordinates.x.update(input_x, timestamp);
    transducer->coordinates.y.update(input_y, timestamp);
    transducer->tip_switch.update(1, timestamp); // wat

    transducer->id = id;
    transducer->secondary_id = id;
}

void VoodooI2CGoodixTouchDriver::stop(IOService* provider) {
    release_resources();
    unpublish_multitouch_interface();
    PMstop();
    IOLog("%s::Stopped\n", getName());
    super::stop(provider);
}

IOReturn VoodooI2CGoodixTouchDriver::setPowerState(unsigned long powerState, IOService* whatDevice) {
    if (whatDevice != this) {
        return kIOReturnInvalid;
    }

    return kIOPMAckImplied;
}

bool VoodooI2CGoodixTouchDriver::publish_multitouch_interface() {
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
    // Goodix's Vendor Id
    mt_interface->setProperty(kIOHIDVendorIDKey, 0x0416, 32);
    mt_interface->setProperty(kIOHIDProductIDKey, 0x0416, 32);
    return true;
multitouch_exit:
    unpublish_multitouch_interface();
    return false;
}

void VoodooI2CGoodixTouchDriver::unpublish_multitouch_interface() {
    if (mt_interface) {
        mt_interface->stop(this);
        mt_interface->release();
        mt_interface = NULL;
    }
}

void VoodooI2CGoodixTouchDriver::release_resources() {
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
}

/* Adapted from the TFE Driver */
IOReturn VoodooI2CGoodixTouchDriver::goodix_read_reg(UInt16 reg, UInt8* values, size_t len) {
    IOReturn retVal = kIOReturnSuccess;
    UInt16 buffer[] {
        OSSwapHostToBigInt16(reg)
    };
    retVal = api->writeReadI2C(reinterpret_cast<UInt8*>(&buffer), sizeof(buffer), values, len);
    return retVal;
}

/* Adapted from the TFE Driver */
IOReturn VoodooI2CGoodixTouchDriver::goodix_write_reg(UInt16 reg, UInt8 value) {
    UInt16 buffer[] {
        OSSwapHostToBigInt16(reg),
        value
    };
    IOReturn retVal = kIOReturnSuccess;
    retVal = api->writeI2C(reinterpret_cast<UInt8*>(&buffer), sizeof(buffer));
    return retVal;
}

/* Ported from goodix.c */
IOReturn VoodooI2CGoodixTouchDriver::goodix_read_version() {
    IOLog("%s::Reading version...\n", getName());
    
    IOReturn retVal = kIOReturnSuccess;
    UInt8 buf[6];
    char id_str[5];

    // AtmelMTX method
    retVal = goodix_read_reg(GOODIX_REG_ID, buf, sizeof(buf));
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::Read version failed: %d\n", getName(), retVal);
        return retVal;
    }

    // Copy the first 4 bytes of the buffer to the id strings
    memcpy(id_str, buf, 4);
    // Reset the last byte of the ID string to zero
    id_str[4] = 0;

    if (!str_to_uint16(id_str, &ts->id)) {
        IOLog("%s::Converting ID to UInt16 failed\n", getName());
        return kIOReturnIOError;
    }

    ts->version = get_unaligned_le16(&buf[4]);

    IOLog("%s::ID %d, version: %04x\n", getName(), ts->id, ts->version);

    return retVal;
}

void VoodooI2CGoodixTouchDriver::goodix_read_config() {
    UInt8 config[GOODIX_CONFIG_MAX_LENGTH];
    IOReturn retVal = kIOReturnSuccess;

    retVal = goodix_read_reg(ts->chip->config_addr, config, ts->chip->config_len);

    if (retVal != kIOReturnSuccess) {
        IOLog("%s::Error reading config (%d), using defaults\n", getName(), retVal);
        ts->abs_x_max = GOODIX_MAX_WIDTH;
        ts->abs_y_max = GOODIX_MAX_HEIGHT;
        if (ts->swapped_x_y)
            swap(ts->abs_x_max, ts->abs_y_max);
        ts->int_trigger_type = GOODIX_INT_TRIGGER;
        ts->max_touch_num = GOODIX_MAX_CONTACTS;
        return;
    }

    ts->abs_x_max = get_unaligned_le16(&config[RESOLUTION_LOC]);
    ts->abs_y_max = get_unaligned_le16(&config[RESOLUTION_LOC + 2]);
    if (ts->swapped_x_y)
        swap(ts->abs_x_max, ts->abs_y_max);

    ts->int_trigger_type = config[TRIGGER_LOC] & 0x03;
    ts->max_touch_num = config[MAX_CONTACTS_LOC] & 0x0f;
    if (!ts->abs_x_max || !ts->abs_y_max || !ts->max_touch_num) {
        IOLog("%s::Invalid config (%d), using defaults\n", getName(), retVal);
        ts->abs_x_max = GOODIX_MAX_WIDTH;
        ts->abs_y_max = GOODIX_MAX_HEIGHT;
        if (ts->swapped_x_y)
            swap(ts->abs_x_max, ts->abs_y_max);
        ts->max_touch_num = GOODIX_MAX_CONTACTS;
    }

    IOLog("%s::Config read successfully\n", getName());

    IOLog("%s::ts->abs_x_max = %d\n", getName(), ts->abs_x_max);
    IOLog("%s::ts->abs_y_max = %d\n", getName(), ts->abs_y_max);
    IOLog("%s::ts->int_trigger_type = %d\n", getName(), ts->int_trigger_type);
    IOLog("%s::ts->max_touch_num = %d\n", getName(), ts->max_touch_num);
}

IOReturn VoodooI2CGoodixTouchDriver::goodix_configure_dev() {
    IOReturn retVal = kIOReturnSuccess;

    // Hardcoded values for Chuwi Minibook 8
    ts->swapped_x_y = false;
    ts->inverted_x = false;
    ts->inverted_y = false;

    goodix_read_config();

    return retVal;
}

bool VoodooI2CGoodixTouchDriver::init_device() {
    ts = (struct goodix_ts_data *)IOMalloc(sizeof(struct goodix_ts_data));
    memset(ts, 0, sizeof(struct goodix_ts_data));

    if (goodix_read_version() != kIOReturnSuccess) {
        return false;
    }

    ts->chip = goodix_get_chip_data(ts->id);

    if (goodix_configure_dev() != kIOReturnSuccess) {
        return false;
    }

    if (mt_interface) {
        mt_interface->physical_max_x = ts->abs_x_max;
        mt_interface->physical_max_y = ts->abs_y_max;
        mt_interface->logical_max_x = ts->abs_x_max;
        mt_interface->logical_max_y = ts->abs_y_max;
    }

    return true;
}
