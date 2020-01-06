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

void VoodooI2CGoodixEventDriver::reportTouches(Touch *touches, int touchCount) {
    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);

    for (int i = 0; i < touchCount; i++) {
        Touch touch = touches[i];

        VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer, transducers->getObject(touch.id));
        transducer->type = kDigitiserTransducerFinger;

        transducer->is_valid = true;

        if (multitouch_interface) {
            transducer->logical_max_x = multitouch_interface->logical_max_x;
            transducer->logical_max_y = multitouch_interface->logical_max_y;
        }

        transducer->coordinates.x.update(touch.x, timestamp);
        transducer->coordinates.y.update(touch.y, timestamp);

        // Todo: do something with touch->width to determine if it's a contact?
        transducer->tip_switch.update(1, timestamp);

        transducer->id = touch.id;
        transducer->secondary_id = touch.id;
    }

    VoodooI2CMultitouchEvent event;
    event.contact_count = touchCount;
    event.transducers = transducers;
    if (multitouch_interface) {
        multitouch_interface->handleInterruptReport(event, timestamp);
    }
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

    transducers = OSArray::withCapacity(GOODIX_MAX_CONTACTS);
    if (!transducers) {
        return false;
    }
    DigitiserTransducerType type = kDigitiserTransducerFinger;
    for (int i = 0; i < GOODIX_MAX_CONTACTS; i++) {
        VoodooI2CDigitiserTransducer* transducer = VoodooI2CDigitiserTransducer::transducer(type, NULL);
        transducers->setObject(transducer);
    }

    setDigitizerProperties();

    PMinit();
    hid_interface->joinPMtree(this);
    registerPowerDriver(this, VoodooI2CIOPMPowerStates, kVoodooI2CIOPMNumberPowerStates);

    return true;
}

void VoodooI2CGoodixEventDriver::handleStop(IOService* provider) {
    if (multitouch_interface) {
        multitouch_interface->stop(this);
        multitouch_interface->detach(this);
        OSSafeReleaseNULL(multitouch_interface);
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
    OSDictionary* properties = OSDictionary::withCapacity(1);

    if (!properties)
        return;

    properties->setObject("Transducer Count", OSNumber::withNumber(GOODIX_MAX_CONTACTS, 32));

    setProperty("Digitizer", properties);
}

IOReturn VoodooI2CGoodixEventDriver::setPowerState(unsigned long whichState, IOService* whatDevice) {
    return kIOPMAckImplied;
}

bool VoodooI2CGoodixEventDriver::start(IOService* provider) {
    if (!super::start(provider))
        return false;

    setProperty("VoodooI2CServices Supported", kOSBooleanTrue);

    return true;
}
