//
//  VoodooI2CGoodixTouchDriver.cpp
//  VoodooI2CGoodix
//
//  Created by CoolStar on 4/24/18.
//  Copyright © 2018 CoolStar. All rights reserved.
//

#include "VoodooI2CGoodixTouchDriver.hpp"

#define super IOService
OSDefineMetaClassAndStructors(VoodooI2CGoodixTouchDriver, IOService);

bool VoodooI2CGoodixTouchDriver::init(OSDictionary *properties) {
    bool res = super::init(properties);
    IOLog("Initializing\n");
    return res;
}

void VoodooI2CGoodixTouchDriver::free() {
    IOLog("Freeing\n");
    super::free();
}

VoodooI2CGoodixTouchDriver *VoodooI2CGoodixTouchDriver::probe(IOService* provider, SInt32* score) {
    super::probe(provider, score);
    IOLog("Probing\n");
    return this;
}

bool VoodooI2CGoodixTouchDriver::start(IOService* provider) {
    bool res = super::start(provider);
    IOLog("Starting\n");
    return res;
}

void VoodooI2CGoodixTouchDriver::stop(IOService* provider) {
    IOLog("Stopping\n");
    super::stop(provider);
}

IOReturn VoodooI2CGoodixTouchDriver::setPowerState(unsigned long powerState, IOService* whatDevice) {
    if (whatDevice != this) {
        return kIOReturnInvalid;
    }

    return kIOPMAckImplied;
}
