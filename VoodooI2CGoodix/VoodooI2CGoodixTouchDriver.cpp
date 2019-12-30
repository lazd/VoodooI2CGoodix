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

bool VoodooI2CGoodixTouchDriver::start(IOService* provider) {
    bool res = super::start(provider);
    IOLog("%s::Starting\n", getName());
    return res;
}

void VoodooI2CGoodixTouchDriver::stop(IOService* provider) {
    IOLog("%s::Stopping\n", getName());
    super::stop(provider);
}

IOReturn VoodooI2CGoodixTouchDriver::setPowerState(unsigned long powerState, IOService* whatDevice) {
    if (whatDevice != this) {
        return kIOReturnInvalid;
    }

    return kIOPMAckImplied;
}
