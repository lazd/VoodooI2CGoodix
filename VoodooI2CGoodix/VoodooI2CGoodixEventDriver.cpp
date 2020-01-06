//
//  VoodooI2CGoodixEventDriver.cpp
//  VoodooI2CGoodix
//
//  Created by Larry Davis on 1/5/20.
//  Copyright © 2020 lazd. All rights reserved.
//

#include "VoodooI2CGoodixEventDriver.hpp"

#include "VoodooI2CGoodixEventDriver.hpp"
#include <IOKit/hid/IOHIDInterface.h>
#include <IOKit/IOLib.h>

#define super IOHIDEventService
OSDefineMetaClassAndStructors(VoodooI2CGoodixEventDriver, IOHIDEventService);

bool VoodooI2CGoodixEventDriver::didTerminate(IOService* provider, IOOptionBits options, bool* defer) {
    if (hid_interface)
        hid_interface->close(this);
    hid_interface = NULL;

    return super::didTerminate(provider, options, defer);
}

void VoodooI2CGoodixEventDriver::forwardReport(VoodooI2CMultitouchEvent event, AbsoluteTime timestamp) {
    if (multitouch_interface)
        multitouch_interface->handleInterruptReport(event, timestamp);
}

const char* VoodooI2CGoodixEventDriver::getProductName() {
    return "Goodix HID Device";
}

bool VoodooI2CGoodixEventDriver::handleStart(IOService* provider) {
    if(!super::handleStart(provider)) {
        return false;
    }

    hid_interface = OSDynamicCast(IOHIDInterface, provider);

    if (!hid_interface)
        return false;

    name = getProductName();

    publishMultitouchInterface();

    digitiser.fingers = OSArray::withCapacity(1);

    if (!digitiser.fingers)
        return false;

    digitiser.styluses = OSArray::withCapacity(1);

    if (!digitiser.styluses)
        return false;

    digitiser.transducers = OSArray::withCapacity(1);

    if (!digitiser.transducers)
        return false;

    setDigitizerProperties();

    PMinit();
    hid_interface->joinPMtree(this);
    registerPowerDriver(this, VoodooI2CIOPMPowerStates, kVoodooI2CIOPMNumberPowerStates);

    return true;
}

void VoodooI2CGoodixEventDriver::handleStop(IOService* provider) {
    OSSafeReleaseNULL(digitiser.transducers);
    OSSafeReleaseNULL(digitiser.wrappers);
    OSSafeReleaseNULL(digitiser.styluses);
    OSSafeReleaseNULL(digitiser.fingers);

    OSSafeReleaseNULL(attached_hid_pointer_devices);

    if (multitouch_interface) {
        multitouch_interface->stop(this);
        multitouch_interface->detach(this);
        OSSafeReleaseNULL(multitouch_interface);
    }

    PMstop();
    super::handleStop(provider);
}

IOReturn VoodooI2CGoodixEventDriver::publishMultitouchInterface() {
    multitouch_interface = OSTypeAlloc(VoodooI2CMultitouchInterface);

    if (!multitouch_interface ||
        !multitouch_interface->init(NULL) ||
        !multitouch_interface->attach(this))
        goto exit;

    if (!multitouch_interface->start(this)) {
        multitouch_interface->detach(this);
        goto exit;
    }

    multitouch_interface->setProperty(kIOHIDVendorIDKey, 0x0416, 32);
    multitouch_interface->setProperty(kIOHIDProductIDKey, 0x0416, 32);

    // Todo: should this be true?
    multitouch_interface->setProperty(kIOHIDDisplayIntegratedKey, kOSBooleanFalse);

    multitouch_interface->registerService();

    return kIOReturnSuccess;

exit:
    OSSafeReleaseNULL(multitouch_interface);
    return kIOReturnError;
}

void VoodooI2CGoodixEventDriver::setDigitizerProperties() {
    OSDictionary* properties = OSDictionary::withCapacity(4);

    if (!properties)
        return;

    if (!digitiser.transducers)
        goto exit;

    properties->setObject("Contact Count Element", digitiser.contact_count);
    properties->setObject("Input Mode Element", digitiser.input_mode);
    properties->setObject("Contact Count Maximum  Element", digitiser.contact_count_maximum);
    properties->setObject("Button Element", digitiser.button);
    properties->setObject("Transducer Count", OSNumber::withNumber(digitiser.transducers->getCount(), 32));

    setProperty("Digitizer", properties);

exit:
    OSSafeReleaseNULL(properties);
}

IOReturn VoodooI2CGoodixEventDriver::setPowerState(unsigned long whichState, IOService* whatDevice) {
    return kIOPMAckImplied;
}

bool VoodooI2CGoodixEventDriver::start(IOService* provider) {
    if (!super::start(provider))
        return false;

    attached_hid_pointer_devices = OSSet::withCapacity(1);

    setProperty("VoodooI2CServices Supported", kOSBooleanTrue);

    return true;
}
