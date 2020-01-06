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

    return super::didTerminate(provider, options, defer);
}

void VoodooI2CGoodixEventDriver::dispatchDigitizerEvent(int logicalX, int logicalY, bool click) {
    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);

    // Convert logical coordinates to IOFixed and Scaled;
    IOFixed x = ((logicalX * 1.0f) / multitouch_interface->logical_max_x) * 65535;
    IOFixed y = ((logicalY * 1.0f) / multitouch_interface->logical_max_y) * 65535;

    // Dispatch the actual event
    dispatchDigitizerEventWithTiltOrientation(timestamp, 0, kDigitiserTransducerFinger, 0x1, click ? 0x1 : 0x0, x, y);

    last_x = x;
    last_y = y;
    last_id = 0;
}

void VoodooI2CGoodixEventDriver::scheduleLift() {
    this->timer_source->setTimeoutMS(14);
}

void VoodooI2CGoodixEventDriver::fingerLift() {
    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);

    dispatchDigitizerEventWithTiltOrientation(timestamp, last_id, kDigitiserTransducerFinger, 0x1, 0x0, last_x, last_y);
}


void VoodooI2CGoodixEventDriver::reportTouches(struct Touch touches[], int numTouches) {
    if (numTouches == 1) {
        // Initial mouse down event
        Touch touch = touches[0];
        dispatchDigitizerEvent(touch.x, touch.y, true);

        // Lift a bit later
        scheduleLift();
    }
    else {
        // Move the cursor to the location of the first finger, but don't click
        dispatchDigitizerEvent(touches[0].x, touches[0].y, false);

        AbsoluteTime timestamp;
        clock_get_uptime(&timestamp);

        // Send a multitouch event for scrolls, scales, etc
        for (int i = 0; i < numTouches; i++) {
            Touch touch = touches[i];

            VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer, transducers->getObject(i));
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

            transducer->id = i;
            transducer->secondary_id = i;
        }

        VoodooI2CMultitouchEvent event;
        event.contact_count = numTouches;
        event.transducers = transducers;
        if (multitouch_interface) {
            multitouch_interface->handleInterruptReport(event, timestamp);
        }
    }
}

const char* VoodooI2CGoodixEventDriver::getProductName() {
    return "Goodix HID Device";
}

bool VoodooI2CGoodixEventDriver::handleStart(IOService* provider) {
    if(!super::handleStart(provider)) {
        return false;
    }

    this->work_loop = getWorkLoop();
    if (!this->work_loop) {
        IOLog("%s::Unable to get workloop\n", getName());
        stop(provider);
        return false;
    }

    work_loop->retain();

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

    multitouch_interface->registerService();

    timer_source = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &VoodooI2CGoodixEventDriver::fingerLift));

    if (!timer_source || work_loop->addEventSource(timer_source) != kIOReturnSuccess) {
        IOLog("%s::Could not add timer source to work loop\n", getName());
        return false;
    }

    return true;
}

void VoodooI2CGoodixEventDriver::handleStop(IOService* provider) {
    unpublishMultitouchInterface();

    if (transducers) {
        for (int i = 0; i < transducers->getCount(); i++) {
            OSObject* object = transducers->getObject(i);
            if (object) {
                object->release();
            }
        }
        OSSafeReleaseNULL(transducers);
    }

    if (timer_source) {
        work_loop->removeEventSource(timer_source);
        OSSafeReleaseNULL(timer_source);
    }

    super::handleStop(provider);
}

IOReturn VoodooI2CGoodixEventDriver::publishMultitouchInterface() {
    multitouch_interface = new VoodooI2CMultitouchInterface();
    if (!multitouch_interface) {
        IOLog("%s::No memory to allocate VoodooI2CMultitouchInterface instance\n", getName());
        goto multitouch_exit;
    }
    if (!multitouch_interface->init(NULL)) {
        IOLog("%s::Failed to init multitouch interface\n", getName());
        goto multitouch_exit;
    }
    if (!multitouch_interface->attach(this)) {
        IOLog("%s::Failed to attach multitouch interface\n", getName());
        goto multitouch_exit;
    }
    if (!multitouch_interface->start(this)) {
        IOLog("%s::Failed to start multitouch interface\n", getName());
        goto multitouch_exit;
    }
    // Assume we are a touchscreen for now
    multitouch_interface->setProperty(kIOHIDDisplayIntegratedKey, true);
    // Goodix's Vendor Id
    multitouch_interface->setProperty(kIOHIDVendorIDKey, 0x0416, 32);
    multitouch_interface->setProperty(kIOHIDProductIDKey, 0x0416, 32);
    IOLog("%s::Published multitouch interface\n", getName());
    return kIOReturnSuccess;
multitouch_exit:
    unpublishMultitouchInterface();
    return kIOReturnError;
}

void VoodooI2CGoodixEventDriver::initializeMultitouchInterface(int x, int y) {
    if (multitouch_interface) {
        IOLog("%s::Initializing multitouch interface with dimensions %d,%d\n", getName(), x, y);
        multitouch_interface->physical_max_x = x;
        multitouch_interface->physical_max_y = y;
        multitouch_interface->logical_max_x = x;
        multitouch_interface->logical_max_y = y;
    }
}

void VoodooI2CGoodixEventDriver::unpublishMultitouchInterface() {
    if (multitouch_interface) {
        IOLog("%s::Unpublishing multitouch interface\n", getName());
        multitouch_interface->stop(this);
        multitouch_interface->release();
        multitouch_interface = NULL;
    }
}

void VoodooI2CGoodixEventDriver::setDigitizerProperties() {
    OSDictionary* properties = OSDictionary::withCapacity(2);

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
