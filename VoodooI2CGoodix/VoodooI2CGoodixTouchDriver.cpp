//
//  VoodooI2CGoodixTouchDriver.cpp
//  VoodooI2CGoodix
//
//  Created by CoolStar on 4/24/18.
//  Copyright © 2018 CoolStar. All rights reserved.
//

#include "VoodooI2CGoodixTouchDriver.hpp"
#include "goodix.h"

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
    const char *cfg_name;
    struct gpio_desc *gpiod_int;
    struct gpio_desc *gpiod_rst;
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
//    interrupt_source = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &VoodooI2CGoodixTouchDriver::interrupt_occurred), api, 0);
//    if (!interrupt_source) {
//        IOLog("%s::Could not get interrupt event source\n", getName());
//        goto start_exit;
//    }
    publish_multitouch_interface();
     if (!init_device()) {
        IOLog("%s::Failed to init device\n", getName());
//        goto start_exit;
    }
    else {
        IOLog("%s::Device initialized\n", getName());
    }
//    workLoop->addEventSource(interrupt_source);
//    interrupt_source->enable();
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

/* Temporary start sequence for testing */
/*
bool VoodooI2CGoodixTouchDriver::start(IOService* provider) {
    if (!super::start(provider)) {
        return false;
    }
    IOLog("%s::Starting\n", getName());
    if (!init_device()) {
        IOLog("%s::Failed to init device\n", getName());
        return false;
    }
    else {
        IOLog("%s::Device initialized\n", getName());
    }
    return true;
}
*/

void VoodooI2CGoodixTouchDriver::stop(IOService* provider) {
//    release_resources();
//    unpublish_multitouch_interface();
//    PMstop();
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

/* Adapted from the AtmelMXT Driver */
IOReturn VoodooI2CGoodixTouchDriver::goodix_read_reg(UInt16 reg, UInt8 *rbuf, int len) {
    // Datasheet indicates that the GT911 takes a 16 bit register address
    // Convert the UInt16 register address into a Uint8 array, respecting endianness
    // https://stackoverflow.com/questions/1289251/converting-a-uint16-value-into-a-uint8-array2/1289360#1289360
    UInt8 wreg[2];
    wreg[0] = reg & 255;
    wreg[1] = reg >> 8;

    IOReturn retVal = kIOReturnSuccess;

    // Use reinterpret_cast to be able to pass the array as if it were a UInt8 pointer
    retVal = api->writeReadI2C(reinterpret_cast<UInt8*>(wreg), sizeof(wreg), rbuf, len);
    return retVal;
}

/* Temp: Taken from the Linux kernel source */
static inline uint16_t __get_unaligned_le16(const uint8_t *p) {
    return p[0] | p[1] << 8;
}

static inline uint16_t get_unaligned_le16(const void *p) {
    return __get_unaligned_le16((const uint8_t *)p);
}

/* Ported from goodix.c */
IOReturn VoodooI2CGoodixTouchDriver::goodix_read_version(struct goodix_ts_data *ts) {
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

    // Todo: Convert id_str to UInt16 (ala kstrtou16)
//    ts->id = id_str;

    ts->version = get_unaligned_le16(&buf[4]);

    IOLog("%s::ID %d, version: %04x\n", getName(), ts->id, ts->version);

    return retVal;
}

IOReturn VoodooI2CGoodixTouchDriver::goodix_get_gpio_config(struct goodix_ts_data *ts) {
    IOReturn retVal = kIOReturnSuccess;

    /*
    // Get the interrupt GPIO pin number
    gpiod = devm_gpiod_get_optional(dev, GOODIX_GPIO_INT_NAME, GPIOD_IN);
    ts->gpiod_int = gpiod;

    // Get the reset line GPIO pin number
    gpiod = devm_gpiod_get_optional(dev, GOODIX_GPIO_RST_NAME, GPIOD_IN);
    ts->gpiod_rst = gpiod;
    */

    return retVal;
}

IOReturn VoodooI2CGoodixTouchDriver::goodix_reset(struct goodix_ts_data *ts) {
    IOReturn retVal = kIOReturnSuccess;

    /*
    // begin select I2C slave addr
    api->gpio_controller->writel(ts->gpiod_rst, 0);

    msleep(20); // T2: > 10ms

    // HIGH: 0x28/0x29, LOW: 0xBA/0xBB
    api->gpio_controller->writel(ts->gpiod_int, 0);

    usleep_range(100, 2000); // T3: > 100us

    api->gpio_controller->writel(ts->gpiod_rst, 1);

    usleep_range(6000, 10000); // T4: > 5ms

    // end select I2C slave addr
    api->gpio_controller->readl(ts->gpiod_rst);

    */

    retVal = goodix_int_sync(ts);
    if (retVal != kIOReturnSuccess) {
        return retVal;
    }

    return retVal;
}

IOReturn VoodooI2CGoodixTouchDriver::goodix_int_sync(struct goodix_ts_data *ts) {
    IOReturn retVal = kIOReturnSuccess;

    /*
    api->gpio_controller->writel(ts->gpiod_int, 0);

    msleep(50); // T5: 50ms

    api->gpio_controller->readl(ts->gpiod_int);
    */

    return retVal;
}

IOReturn VoodooI2CGoodixTouchDriver::goodix_configure_dev(struct goodix_ts_data *ts) {
    IOReturn retVal = kIOReturnSuccess;

    return retVal;
}

IOReturn VoodooI2CGoodixTouchDriver::goodix_read_config(struct goodix_ts_data *ts) {
    IOReturn retVal = kIOReturnSuccess;

    return retVal;
}

bool VoodooI2CGoodixTouchDriver::init_device() {
    IOReturn retVal = kIOReturnSuccess;
    struct goodix_ts_data *ts;
    ts = (struct goodix_ts_data *)IOMalloc(sizeof(struct goodix_ts_data));
    memset(ts, 0, sizeof(struct goodix_ts_data));

    if (goodix_get_gpio_config(ts) != kIOReturnSuccess) {
        return retVal;
    }

    if (ts->gpiod_int && ts->gpiod_rst) {
        // Reset the controller
        // It's unclear whether this is required for normal operation, or firmware udpates only
        if (goodix_reset(ts) != kIOReturnSuccess) {
            return false;
        }
    }

    if (goodix_read_version(ts) != kIOReturnSuccess) {
        return false;
    }

    ts->chip = goodix_get_chip_data(ts->id);

    if (ts->gpiod_int && ts->gpiod_rst) {
        // update device config
        // In goodix.c, this routine would call request_firmware_nowait() and subsequently goodix_config_cb()
        // It seems this is a routine to update firmware, but unsure
    }
    else {
        if (goodix_configure_dev(ts) != kIOReturnSuccess) {
            return false;
        }
    }

    return true;
}
