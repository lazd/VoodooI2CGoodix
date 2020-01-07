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

void VoodooI2CGoodixEventDriver::dispatchDigitizerEvent(int logicalX, int logicalY, UInt32 clickType) {
    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);

    // Convert logical coordinates to IOFixed and Scaled;
    IOFixed x = ((logicalX * 1.0f) / multitouch_interface->logical_max_x) * 65535;
    IOFixed y = ((logicalY * 1.0f) / multitouch_interface->logical_max_y) * 65535;

    // Dispatch the actual event
    dispatchDigitizerEventWithTiltOrientation(timestamp, 0, kDigitiserTransducerFinger, 0x1, clickType, x, y);

    // Store the coordinates so we can lift the finger later
    last_x = x;
    last_y = y;
}

void VoodooI2CGoodixEventDriver::scheduleLift() {
    this->timer_source->cancelTimeout();
    this->timer_source->setTimeoutMS(FINGER_LIFT_EVENT_DELAY);
}

void VoodooI2CGoodixEventDriver::fingerLift() {
    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);

    dispatchDigitizerEventWithTiltOrientation(timestamp, 0, kDigitiserTransducerFinger, 0x1, HOVER, last_x, last_y);

    click_tick = 0;
    start_scroll = true;

    // Reset right click status so we're not stuck checking if we're still right clicking
    if (right_click) {
        right_click = false;
    }
}

void VoodooI2CGoodixEventDriver::reportTouches(struct Touch touches[], int numTouches) {
    if (numTouches == 1) {
        Touch touch = touches[0];

        // Check for right clicks
        UInt16 temp_x = touch.x;
        UInt16 temp_y = touch.y;
        if (!right_click) {
            if (temp_x == compare_input_x && temp_y == compare_input_y) {
                compare_input_counter = compare_input_counter + 1;
                compare_input_x = temp_x;
                compare_input_y = temp_y;

                if (compare_input_counter >= RIGHT_CLICK_TICKS && !right_click) {
                    compare_input_x = 0;
                    compare_input_y = 0;
                    compare_input_counter = 0;
                    right_click = true;
                }
            }
            else {
                compare_input_x = temp_x;
                compare_input_y = temp_y;
                compare_input_counter = 0;
            }
        }

        // Determine what click type to send
        UInt32 clickType = LEFT_CLICK;

        if (click_tick <= HOVER_TICKS) {
            clickType = HOVER;
            click_tick++;
        }

        if (right_click) {
            clickType = right_click;
        }

        dispatchDigitizerEvent(touch.x, touch.y, clickType);

        // Lift the finger a bit later
        scheduleLift();
    }
    else {
        // Our finger event is multitouch, so we're not clicking
        click_tick = 0;

        // Move the cursor to the location of the first finger, but don't click
        dispatchDigitizerEvent(touches[0].x, touches[0].y, HOVER);

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

    publishMultitouchInterface();

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
    IOLog("%s::Published multitouch interface\n", getName());
    return kIOReturnSuccess;
multitouch_exit:
    unpublishMultitouchInterface();
    return kIOReturnError;
}

void VoodooI2CGoodixEventDriver::configureMultitouchInterface(int logicalMaxX, int logicalMaxY, int numTransducers, UInt32 vendorId) {
    if (multitouch_interface) {
        IOLog("%s::Configuring multitouch interface with dimensions %d,%d and %d transducers\n", getName(), logicalMaxX, logicalMaxY, numTransducers);

        transducers = OSArray::withCapacity(numTransducers);
        if (!transducers) {
            IOLog("%s::No memory to allocate transducers array\n", getName());
            return;
        }
        DigitiserTransducerType type = kDigitiserTransducerFinger;
        for (int i = 0; i < numTransducers; i++) {
            VoodooI2CDigitiserTransducer* transducer = VoodooI2CDigitiserTransducer::transducer(type, NULL);
            transducers->setObject(transducer);
        }

        multitouch_interface->physical_max_x = logicalMaxX;
        multitouch_interface->physical_max_y = logicalMaxY;
        multitouch_interface->logical_max_x = logicalMaxX;
        multitouch_interface->logical_max_y = logicalMaxY;

        multitouch_interface->setProperty(kIOHIDVendorIDKey, vendorId, 32);
        multitouch_interface->setProperty(kIOHIDProductIDKey, vendorId, 32);

        setProperty(kIOHIDVendorIDKey, vendorId, 32);
        setProperty(kIOHIDProductIDKey, vendorId, 32);

        OSDictionary* properties = OSDictionary::withCapacity(2);

        if (!properties) {
            IOLog("%s::No memory to allocate properties dictionary\n", getName());
            return;
        }

        properties->setObject("Transducer Count", OSNumber::withNumber(numTransducers, 32));

        setProperty("Digitizer", properties);
    }
}

void VoodooI2CGoodixEventDriver::unpublishMultitouchInterface() {
    if (multitouch_interface) {
        multitouch_interface->stop(this);
        multitouch_interface->release();
        multitouch_interface = NULL;
    }
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
