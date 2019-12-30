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

void VoodooI2CGoodixTouchDriver::free() {
    super::free();
}

bool VoodooI2CGoodixTouchDriver::init(OSDictionary *properties) {
    awake = true;
    ready_for_input = false;
    read_in_progress = false;
    return true;
}

VoodooI2CGoodixTouchDriver* VoodooI2CGoodixTouchDriver::probe(IOService* provider, SInt32* score) {
    return NULL;
}

bool VoodooI2CGoodixTouchDriver::start(IOService* provider) {
    if (!super::start(provider)) {
        return false;
    }

    return true;
}

void VoodooI2CGoodixTouchDriver::stop(IOService* provider) {
    super::stop(provider);
}

IOReturn VoodooI2CGoodixTouchDriver::setPowerState(unsigned long powerState, IOService* whatDevice) {
    if (whatDevice != this) {
        return kIOReturnInvalid;
    }

    if (powerState == 0){
        awake = false;
    }
    else {
        if (!awake) {
            awake = true;
        }
    }
    return kIOPMAckImplied;
}
