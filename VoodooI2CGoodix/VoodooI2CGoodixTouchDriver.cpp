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
    UInt16 checksum_addr;
    UInt16 config_fresh_addr;
};

struct goodix_ts_data {
    const struct goodix_chip_data *chip;
    int abs_x_max;
    int abs_y_max;
    bool swapped_x_y;
    bool inverted_x;
    bool inverted_y;
    unsigned int max_touch_num;
    UInt16 id;
    UInt16 version;
};

static const struct goodix_chip_data gt1x_chip_data = {
    .config_addr        = GOODIX_GT1X_REG_CONFIG_DATA,
    .config_len         = GOODIX_CONFIG_MAX_LENGTH,
    .checksum_addr      = GOODIX_CONFIG_MAX_LENGTH - 2,
    .config_fresh_addr  = GOODIX_CONFIG_MAX_LENGTH - 1
};

static const struct goodix_chip_data gt911_chip_data = {
    .config_addr        = GOODIX_GT9X_REG_CONFIG_DATA,
    .config_len         = GOODIX_CONFIG_911_LENGTH,
    .checksum_addr      = GOODIX_CONFIG_911_LENGTH - 2,
    .config_fresh_addr  = GOODIX_CONFIG_911_LENGTH - 1
};

static const struct goodix_chip_data gt967_chip_data = {
    .config_addr        = GOODIX_GT9X_REG_CONFIG_DATA,
    .config_len         = GOODIX_CONFIG_967_LENGTH,
    .checksum_addr      = GOODIX_CONFIG_967_LENGTH - 2,
    .config_fresh_addr  = GOODIX_CONFIG_967_LENGTH - 1
};

static const struct goodix_chip_data gt9x_chip_data = {
    .config_addr        = GOODIX_GT9X_REG_CONFIG_DATA,
    .config_len         = GOODIX_CONFIG_MAX_LENGTH,
    .checksum_addr      = GOODIX_CONFIG_MAX_LENGTH - 2,
    .config_fresh_addr  = GOODIX_CONFIG_MAX_LENGTH - 1
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
    if (!super::init(properties)) {
        return false;
    }

    numTouches = 0;
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
        IOLog("%s::Could not get ACPI device\n", getName());
        return NULL;
    }
    acpi_device->evaluateObject("_PS0");
    
    api = OSDynamicCast(VoodooI2CDeviceNub, provider);
    if (!api) {
        IOLog("%s::Could not get VoodooI2C API instance\n", getName());
        return NULL;
    }
    return this;
}

bool VoodooI2CGoodixTouchDriver::start(IOService* provider) {
    if (!super::start(provider)) {
        return false;
    }
    bool event_driver_initialized = true;
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

    // Instantiate the event driver
    event_driver = OSTypeAlloc(VoodooI2CGoodixEventDriver);
    if (!event_driver
        || !event_driver->init()
        || !event_driver->attach(this)
    ) {
        event_driver_initialized = false;
    }
    else if (!event_driver->start(this)) {
        event_driver->detach(this);
        event_driver_initialized = false;
    }

    if (!event_driver_initialized) {
        IOLog("%s::Could not initialise event_driver\n", getName());
        OSSafeReleaseNULL(event_driver);
        goto start_exit;
    }

    event_driver->configureMultitouchInterface(ts->abs_x_max, ts->abs_y_max, GOODIX_MAX_CONTACTS, GOODIX_VENDOR_ID);
    event_driver->registerService();

    registerService();

    return true;
start_exit:
    release_resources();
    return false;
}

void VoodooI2CGoodixTouchDriver::interrupt_occurred(OSObject* owner, IOInterruptEventSource* src, int intCount) {
    if (read_in_progress || !awake) {
        return;
    }
    interrupt_source->disable();
    read_in_progress = true;
    thread_t new_thread;
    kern_return_t ret = kernel_thread_start(OSMemberFunctionCast(thread_continue_t, this, &VoodooI2CGoodixTouchDriver::handle_input_threaded), this, &new_thread);
    if (ret != KERN_SUCCESS) {
        read_in_progress = false;
        interrupt_source->enable();
        IOLog("%s::Thread error while attemping to get input report: %d\n", getName(), ret);
    }
    else {
        thread_deallocate(new_thread);
    }
}

void VoodooI2CGoodixTouchDriver::handle_input_threaded() {
    if (!ready_for_input || !command_gate) {
        interrupt_source->enable(); // Fix initialization problem
        read_in_progress = false;
        return;
    }
    command_gate->attemptAction(OSMemberFunctionCast(IOCommandGate::Action, this, &VoodooI2CGoodixTouchDriver::goodix_process_events));
    goodix_end_cmd();
    interrupt_source->enable();
    read_in_progress = false;
}

IOReturn VoodooI2CGoodixTouchDriver::goodix_end_cmd() {
    IOReturn retVal = goodix_write_reg(GOODIX_READ_COOR_ADDR, 0);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::I2C write end_cmd 0 error: %d\n", getName(), retVal);
    }
    return retVal;
}

/* Ported from goodix.c */
IOReturn VoodooI2CGoodixTouchDriver::goodix_process_events() {
    // Allocate enough space for the status byte, all touches, and the extra button byte
    UInt8 data[1 + GOODIX_CONTACT_SIZE * GOODIX_MAX_CONTACTS + 1];

    numTouches = goodix_ts_read_input_report(data);
    if (numTouches <= 0) {
        return kIOReturnSuccess;
    }
    
    
    if (numTouches == 1) {
        UInt8 *pd0 = &data[1];
        int input_x = get_unaligned_le16(&pd0[1]);
        int input_y = get_unaligned_le16(&pd0[3]);
        
        input_x += 5; // Shift x up 5 pixel for better experience
        pd0[1] = input_x & 0xFF;
        pd0[2] = (input_x & 0xFF00) >> 8;
        
        if (!event_driver->isDoNotSimScroll()) {
            if ((event_driver->getCurrentInteractionType() == LEFT_CLICK &&
                (input_x != event_driver->getNextLogicalX() ||
                input_y != event_driver->getNextLogicalY())) || event_driver->isScrollStarted()) {
                // Dragging, simulate scroll by faking two finger tap
                UInt8 *pd1 = &data[1 + GOODIX_CONTACT_SIZE];

                short x1 = input_x - 50; // simulate horizontal two finger tap
                short x2 = input_x + 50;
                pd0[1] = x1 & 0xFF;
                pd0[2] = (x1 & 0xFF00) >> 8;
                pd1[1] = x2 & 0xFF;
                pd1[2] = (x2 & 0xFF00) >> 8;
                
                pd1[0] = ((pd0[0] & 0x0F) + 1) + (pd0[0] & 0xF0);
                pd1[3] = pd0[3];
                pd1[4] = pd0[4];
                pd1[5] = pd0[5];
                pd1[6] = pd0[6];
                pd1[7] = pd0[7];
                
                numTouches = 2;
            }
        }
    }
    

    UInt8 keys = data[1 + numTouches * GOODIX_CONTACT_SIZE];
    if (GOODIX_KEYDOWN_EVENT(keys)) {
        stylusButton1 = GOODIX_IS_STYLUS_BTN_DOWN(keys, GOODIX_STYLUS_BTN1);
        stylusButton2 = GOODIX_IS_STYLUS_BTN_DOWN(keys, GOODIX_STYLUS_BTN2);
    }
    else {
        stylusButton1 = false;
        stylusButton2 = false;
    }

    UInt8 *point_data;
    for (int i = 0; i < numTouches; i++) {
        point_data = &data[1 + i * GOODIX_CONTACT_SIZE];
        if (numTouches == 2) {
		    // Let touchscreen scroll direction oppsite to trackpad, for better user experience
            bool ivtxold = ts->inverted_x;
            bool ivtyold = ts->inverted_y;
            ts->inverted_x = true;
            ts->inverted_y = true;
            goodix_ts_store_touch(point_data);
            ts->inverted_x = ivtxold;
            ts->inverted_y = ivtyold;
        } else {
            goodix_ts_store_touch(point_data);
        }
    }

    if (numTouches > 0) {
        // send the event into the event driver
        event_driver->reportTouches(touches, numTouches, stylusButton1, stylusButton2);
    }

    return kIOReturnSuccess;
}

/* Ported from goodix.c */
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

        // On the intial read, get the status byte, the first touch, and the pen buttons
        retVal = goodix_read_reg(GOODIX_READ_COOR_ADDR, data, 1 + GOODIX_CONTACT_SIZE + 1);
        if (retVal != kIOReturnSuccess) {
            IOLog("%s::I2C transfer error starting coordinate read: %d\n", getName(), retVal);
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
                // Read all touches, and 1 additional byte for the pen buttons
                retVal = goodix_read_reg(GOODIX_READ_COOR_ADDR + 1 + GOODIX_CONTACT_SIZE, data, GOODIX_CONTACT_SIZE * (touch_num - 1) + 1);
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

/* Ported from goodix.c */
void VoodooI2CGoodixTouchDriver::goodix_ts_store_touch(UInt8 *coor_data) {
    int id = coor_data[0] & 0x0F;
    int input_x = get_unaligned_le16(&coor_data[1]);
    int input_y = get_unaligned_le16(&coor_data[3]);
    int input_w = get_unaligned_le16(&coor_data[5]);
    bool type = GOODIX_TOOL_TYPE(coor_data[0]) == GOODIX_TOOL_PEN;

    // Inversions have to happen before axis swapping
    if (ts->inverted_x) {
#ifdef GOODIX_TOUCH_DRIVER_DEBUG
        IOLog("%s:: inverted_x %d, %d\n", getName(), input_x, ts->abs_x_max);
#endif
        input_x = ts->abs_x_max - input_x;
    }
    if (ts->inverted_y) {
#ifdef GOODIX_TOUCH_DRIVER_DEBUG
        IOLog("%s:: inverted_y %d, %d\n", getName(), input_y, ts->abs_y_max);
#endif
        input_y = ts->abs_y_max - input_y;
    }

    if (ts->swapped_x_y) {
#ifdef GOODIX_TOUCH_DRIVER_DEBUG
        IOLog("%s:: swapped_x_y %d, %d\n", getName(), input_x, input_y);
#endif
        swap(input_x, input_y);
    }

#ifdef GOODIX_TOUCH_DRIVER_DEBUG
    IOLog("%s::%s %d with width %d at %d,%d\n", getName(), type ? "Stylus" : "Touch", id, input_w, input_x, input_y);
#endif

    // Store touch information
    touches[id].x = input_x;
    touches[id].y = input_y;
    touches[id].width = input_w;
    touches[id].type = type;
}

void VoodooI2CGoodixTouchDriver::stop(IOService* provider) {
    release_resources();

    PMstop();
    IOLog("%s::Stopped\n", getName());
    super::stop(provider);
}

IOReturn VoodooI2CGoodixTouchDriver::setPowerState(unsigned long whichState, IOService* whatDevice) {
    #ifndef GOODIX_TOUCH_DRIVER_DEBUG
    if (whichState == 0) {
        if (awake) {
            awake = false;
            while (read_in_progress) {
                IOLog("%s::Waiting for read to finish before sleeping...\n", getName());
                IOSleep(10);
            }
            IOLog("%s::Going to sleep\n", getName());
        }
    }
    else {
        if (!awake) {
            awake = true;
            IOLog("%s::Waking up\n", getName());
        }
    }
    #endif

    return kIOPMAckImplied;
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
    if (event_driver) {
        event_driver->stop(this);
        event_driver->detach(this);
        OSSafeReleaseNULL(event_driver);
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

/* Ported from goodix.c */
void VoodooI2CGoodixTouchDriver::goodix_read_config() {
    IOLog("%s::Reading config...\n", getName());

    UInt8 config[GOODIX_CONFIG_MAX_LENGTH];
    IOReturn retVal = kIOReturnSuccess;

    retVal = goodix_read_reg(ts->chip->config_addr, config, ts->chip->config_len);
    if (retVal != kIOReturnSuccess) {
        IOLog("%s::Error reading config (%d), using defaults\n", getName(), retVal);
    }
    else {
        retVal = goodix_check_config(config);
        if (retVal != kIOReturnSuccess) {
            IOLog("%s::Config checksum mismatch, using defaults\n", getName());
        }
    }

    if (retVal != kIOReturnSuccess) {
        set_default_config();
        return;
    }

    ts->abs_x_max = get_unaligned_le16(&config[RESOLUTION_LOC]);
    ts->abs_y_max = get_unaligned_le16(&config[RESOLUTION_LOC + 2]);
    if (ts->swapped_x_y)
        swap(ts->abs_x_max, ts->abs_y_max);
    ts->max_touch_num = config[MAX_CONTACTS_LOC] & 0x0f;

    if (!ts->abs_x_max || !ts->abs_y_max || !ts->max_touch_num) {
        IOLog("%s::Config is missing information, using defaults\n", getName());
        set_default_config();
        return;
    }

    // These configuration values are not required for operation
    // But they may be useful for debugging, so dump them
    UInt8 screenTouchLevel = config[SCREEN_TOUCH_LEVEL_LOC] & 0xff;
    UInt8 screenLeaveLevel = config[LEAVE_LEVEL_LOC];
    UInt8 lowPowerInterval = config[LOW_POWER_INTERVAL_LOC] & 0x0f;
    UInt8 refreshRate = config[REFRESH_LOC] & 0x0f;
    UInt8 xThreshold = config[X_THRESHOLD_LOC];
    UInt8 yThreshold = config[Y_THRESHOLD_LOC];

    IOLog("%s::xOutputMax = %d\n", getName(), ts->abs_x_max);
    IOLog("%s::yOutputMax = %d\n", getName(), ts->abs_y_max);
    IOLog("%s::maxTouches = %d\n", getName(), ts->max_touch_num);
    IOLog("%s::screenTouchLevel = %d\n", getName(), screenTouchLevel);
    IOLog("%s::screenLeaveLevel = %d\n", getName(), screenLeaveLevel);
    IOLog("%s::lowPowerInterval = %d\n", getName(), lowPowerInterval);
    IOLog("%s::refreshRate = %d\n", getName(), refreshRate);
    IOLog("%s::xThreshold = %d\n", getName(), xThreshold);
    IOLog("%s::yThreshold = %d\n", getName(), yThreshold);
}

void VoodooI2CGoodixTouchDriver::set_default_config() {
    ts->abs_x_max = GOODIX_MAX_WIDTH;
    ts->abs_y_max = GOODIX_MAX_HEIGHT;
    if (ts->swapped_x_y)
        swap(ts->abs_x_max, ts->abs_y_max);
    ts->max_touch_num = GOODIX_MAX_CONTACTS;
}

UInt8 VoodooI2CGoodixTouchDriver::goodix_calculate_config_checksum(UInt8 config[]) {
    UInt8 checksum = 0;
    for (int i = 0; i < ts->chip->checksum_addr; i++) {
        checksum += config[i];
    }
    checksum = (~checksum) + 1;
    return checksum;
}

IOReturn VoodooI2CGoodixTouchDriver::goodix_check_config(UInt8 config[]) {
    UInt8 storedChecksum = config[ts->chip->checksum_addr];
    UInt8 actualChecksum = goodix_calculate_config_checksum(config);

    if (storedChecksum != actualChecksum) {
        IOLog("%s::Config checksum (%d) does not match stored checksum (%d)\n", getName(), actualChecksum, storedChecksum);
        return kIOReturnIOError;
    }
    return kIOReturnSuccess;
}

IOReturn VoodooI2CGoodixTouchDriver::goodix_configure_dev() {
    IOReturn retVal = kIOReturnSuccess;

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

    return true;
}
