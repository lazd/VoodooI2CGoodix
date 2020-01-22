//
//  VoodooI2CGoodixEventDriver.cpp
//  VoodooI2CGoodix
//
//  Created by Larry Davis on 1/5/20.
//  Copyright © 2020 lazd. All rights reserved.
//

#include "VoodooI2CGoodixEventDriver.hpp"
#include <IOKit/hid/IOHIDInterface.h>
#include <IOKit/IOLib.h>

#define super IOHIDEventService
OSDefineMetaClassAndStructors(VoodooI2CGoodixEventDriver, IOHIDEventService);

static UInt64 getNanoseconds() {
    AbsoluteTime timestamp;
    UInt64 nanoseconds;
    clock_get_uptime(&timestamp);
    absolutetime_to_nanoseconds(timestamp, &nanoseconds);
    return nanoseconds;
}

void VoodooI2CGoodixEventDriver::dispatchPenEvent(int logicalX, int logicalY, int pressure, UInt32 clickType) {
    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);

    // Convert logical coordinates to IOFixed and Scaled;
    IOFixed x = ((logicalX * 1.0f) / multitouch_interface->logical_max_x) * 65535;
    IOFixed y = ((logicalY * 1.0f) / multitouch_interface->logical_max_y) * 65535;
    IOFixed tipPressure = ((pressure * 1.0f) / 1024) * 65535;

    checkRotation(&x, &y);

    // Dispatch the actual event
    dispatchDigitizerEventWithTiltOrientation(timestamp, 0, kDigitiserTransducerStylus, 0x1, clickType, x, y, 65535, tipPressure);

    // Store the coordinates so we can lift the finger later
    lastEventFixedX = x;
    lastEventFixedY = y;
}

void VoodooI2CGoodixEventDriver::dispatchDigitizerEvent(int logicalX, int logicalY, UInt32 clickType) {
    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);

    // Convert logical coordinates to IOFixed and Scaled;
    IOFixed x = ((logicalX * 1.0f) / multitouch_interface->logical_max_x) * 65535;
    IOFixed y = ((logicalY * 1.0f) / multitouch_interface->logical_max_y) * 65535;

    checkRotation(&x, &y);

    // Dispatch the actual event
    dispatchDigitizerEventWithTiltOrientation(timestamp, 0, kDigitiserTransducerFinger, 0x1, clickType, x, y);

    // Store the coordinates so we can lift the finger later
    lastEventFixedX = x;
    lastEventFixedY = y;
}

void VoodooI2CGoodixEventDriver::scheduleLift() {
    this->liftTimerSource->cancelTimeout();
    this->liftTimerSource->setTimeoutMS(FINGER_LIFT_DELAY);
}

void VoodooI2CGoodixEventDriver::fingerLift() {
    #ifdef GOODIX_EVENT_DRIVER_DEBUG
    IOLog("%s::Finger lifted\n", getName());
    #endif

    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);

    dispatchDigitizerEventWithTiltOrientation(timestamp, 0, kDigitiserTransducerFinger, 0x1, HOVER, lastEventFixedX, lastEventFixedY);

    // Mark that the finger has been lifted
    fingerDown = false;
    currentInteractionType = HOVER;

    scrollStarted = false;

    // Reset all transducers
    for (int i = 0; i < transducers->getCount(); i++) {
        VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer, transducers->getObject(i));

        transducer->tip_switch.update(0, timestamp);
    }

    VoodooI2CMultitouchEvent event;
    event.contact_count = 0;
    event.transducers = transducers;
    if (multitouch_interface) {
        multitouch_interface->handleInterruptReport(event, timestamp);
    }
}

void VoodooI2CGoodixEventDriver::handleSingletouchInteraction(Touch touch, bool stylusButton1, bool stylusButton2) {
    int logicalX = touch.x;
    int logicalY = touch.y;
    int width = touch.width;
    bool isStylus = touch.type;

    if (isStylus) {
        UInt8 type = HOVER;

        if (width == 0) {
            #ifdef GOODIX_EVENT_DRIVER_DEBUG
            IOLog("%s::Stylus hovering at %d, %d \n", getName(), logicalX, logicalY);
            #endif
        }
        else {
            if (stylusButton1) {
                type = RIGHT_CLICK;
                #ifdef GOODIX_EVENT_DRIVER_DEBUG
                IOLog("%s::Stylus right click at %d, %d \n", getName(), logicalX, logicalY);
                #endif
            }
            else {
                type = LEFT_CLICK;
                #ifdef GOODIX_EVENT_DRIVER_DEBUG
                IOLog("%s::Stylus left click at %d, %d \n", getName(), logicalX, logicalY);
                #endif
            }

            scheduleLift();
        }

        dispatchPenEvent(logicalX, logicalY, width, type);

        return;
    }

    UInt64 nanoseconds = getNanoseconds();

    if (fingerDown) {
        if (logicalX == nextLogicalX && logicalY == nextLogicalY) {
            if (currentInteractionType == DRAG) {
                #ifdef GOODIX_EVENT_DRIVER_DEBUG
                IOLog("%s::Still dragging at %d, %d\n", getName(), nextLogicalX, nextLogicalY);
                #endif

                // Keep on dragging
                dispatchDigitizerEvent(logicalX, logicalY, LEFT_CLICK);
            }
            else {
                #ifdef GOODIX_EVENT_DRIVER_DEBUG
                IOLog("%s::Still hovering at %d, %d\n", getName(), nextLogicalX, nextLogicalY);
                #endif

                // Check for a right click
                if (currentInteractionType == RIGHT_CLICK) {
                    // Keep the right mousebutton down in the same place
                    dispatchDigitizerEvent(logicalX, logicalY, RIGHT_CLICK);
                }
                else {
                    // Hover in the same place
                    dispatchDigitizerEvent(logicalX, logicalY, HOVER);

                    UInt64 elapsed = (nanoseconds - fingerDownStart) / 1000000;
                    if (elapsed > RIGHT_CLICK_DELAY) {
                        // Cancel our outstanding click, we're right clicking now
                        this->clickTimerSource->cancelTimeout();

                        #ifdef GOODIX_EVENT_DRIVER_DEBUG
                        IOLog("%s::Right click at %d, %d\n", getName(), logicalX, logicalY);
                        #endif

                        dispatchDigitizerEvent(logicalX, logicalY, RIGHT_CLICK);
                        currentInteractionType = RIGHT_CLICK;
                    }
                }
            }
        }
        else {
            if (currentInteractionType == LEFT_CLICK) {
                // Cancel our outstanding click, we're dragging now
                this->clickTimerSource->cancelTimeout();

                #ifdef GOODIX_EVENT_DRIVER_DEBUG
                IOLog("%s::Begin dragging at %d, %d\n", getName(), nextLogicalX, nextLogicalY);
                #endif

                // Issue a mousedown where we were before
                dispatchDigitizerEvent(nextLogicalX, nextLogicalY, LEFT_CLICK);

                currentInteractionType = DRAG;
            }

            #ifdef GOODIX_EVENT_DRIVER_DEBUG
            IOLog("%s::Dragging at %d, %d\n", getName(), logicalX, logicalY);
            #endif

            // Report that we moved with the mousedown
            if (currentInteractionType == RIGHT_CLICK) {
                dispatchDigitizerEvent(logicalX, logicalY, RIGHT_CLICK);
            }
            else {
                dispatchDigitizerEvent(logicalX, logicalY, LEFT_CLICK);
            }

            // Store the coordinates for the next tick
            nextLogicalX = logicalX;
            nextLogicalY = logicalY;
        }
    }
    else {
        // The finger just hit the screen, so store the start time
        fingerDownStart = nanoseconds;
        fingerDown = true;
        nextLogicalX = logicalX;
        nextLogicalY = logicalY;
        currentInteractionType = LEFT_CLICK;

        // Every interaction could be a click, so check for one in a bit
        scheduleClickCheck();

        #ifdef GOODIX_EVENT_DRIVER_DEBUG
        IOLog("%s::Began hover at %d, %d\n", getName(), nextLogicalX, nextLogicalY);
        #endif

        dispatchDigitizerEvent(logicalX, logicalY, HOVER);
    }

    // No matter what, we need to ensure we issue a mouseup after some time
    scheduleLift();
}

void VoodooI2CGoodixEventDriver::scheduleClickCheck() {
    this->clickTimerSource->cancelTimeout();
    this->clickTimerSource->setTimeoutMS(CLICK_DELAY);
}

void VoodooI2CGoodixEventDriver::checkForClick() {
    if (!fingerDown) {
        #ifdef GOODIX_EVENT_DRIVER_DEBUG
        IOLog("%s::Executing a click at %d, %d\n", getName(), nextLogicalX, nextLogicalY);
        #endif

        // The finger was lifted within click time, which means we have a click!
        dispatchDigitizerEvent(nextLogicalX, nextLogicalY, LEFT_CLICK);

        // Immediately lift, we're doing this quick status since we already waited
        dispatchDigitizerEvent(nextLogicalX, nextLogicalY, HOVER);
    }
}

void VoodooI2CGoodixEventDriver::handleMultitouchInteraction(struct Touch touches[], int numTouches) {
    // Set rotation for gestures
    multitouch_interface->setProperty(kIOFBTransformKey, currentRotation, 8);

    if (numTouches == 2 && !scrollStarted) {
        // Move the cursor to the location between the two fingers
        dispatchDigitizerEvent((touches[0].x + touches[1].x) / 2, (touches[0].y + touches[1].y) / 2, HOVER);

        scrollStarted = true;
    }

    AbsoluteTime timestamp;
    clock_get_uptime(&timestamp);

    // Send a multitouch event for scrolls, scales, etc
    for (int i = 0; i < numTouches; i++) {
        Touch touch = touches[i];

        VoodooI2CDigitiserTransducer* transducer = OSDynamicCast(VoodooI2CDigitiserTransducer, transducers->getObject(i));
        transducer->coordinates.x.update(touch.x, timestamp);
        transducer->coordinates.y.update(touch.y, timestamp);

        transducer->is_valid = true; // Todo: is this required?
        transducer->tip_switch.update(1, timestamp);
    }

    VoodooI2CMultitouchEvent event;
    event.contact_count = numTouches;
    event.transducers = transducers;
    if (multitouch_interface) {
        multitouch_interface->handleInterruptReport(event, timestamp);
    }

    // Make sure we schedule a lift for when the gesture ends to reset state
    scheduleLift();
}

void VoodooI2CGoodixEventDriver::reportTouches(struct Touch touches[], int numTouches, bool stylusButton1, bool stylusButton2) {
    if (!activeFramebuffer) {
        activeFramebuffer = getFramebuffer();
    }

    if (activeFramebuffer) {
        OSNumber* number = OSDynamicCast(OSNumber, activeFramebuffer->getProperty(kIOFBTransformKey));
        currentRotation = number->unsigned8BitValue() / 0x10;
    }

    if (numTouches == 1) {
        handleSingletouchInteraction(touches[0], stylusButton1, stylusButton2);
    }
    else {
        handleMultitouchInteraction(touches, numTouches);
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

    activeFramebuffer = getFramebuffer();

    liftTimerSource = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &VoodooI2CGoodixEventDriver::fingerLift));
    if (!liftTimerSource || work_loop->addEventSource(liftTimerSource) != kIOReturnSuccess) {
        IOLog("%s::Could not add lift timer source to work loop\n", getName());
        return false;
    }

    clickTimerSource = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &VoodooI2CGoodixEventDriver::checkForClick));
    if (!clickTimerSource || work_loop->addEventSource(clickTimerSource) != kIOReturnSuccess) {
        IOLog("%s::Could not add click timer source to work loop\n", getName());
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

    if (liftTimerSource) {
        liftTimerSource->cancelTimeout();
        work_loop->removeEventSource(liftTimerSource);
        OSSafeReleaseNULL(liftTimerSource);
    }

    if (clickTimerSource) {
        clickTimerSource->cancelTimeout();
        work_loop->removeEventSource(clickTimerSource);
        OSSafeReleaseNULL(clickTimerSource);
    }

    OSSafeReleaseNULL(work_loop);

//    OSSafeReleaseNULL(activeFramebuffer); // Todo: do we need to do this?

    super::handleStop(provider);
}

IOReturn VoodooI2CGoodixEventDriver::publishMultitouchInterface() {
    multitouch_interface = OSTypeAlloc(VoodooI2CMultitouchInterface);
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

        multitouch_interface->physical_max_x = logicalMaxX;
        multitouch_interface->physical_max_y = logicalMaxY;
        multitouch_interface->logical_max_x = logicalMaxX;
        multitouch_interface->logical_max_y = logicalMaxY;

        multitouch_interface->setProperty(kIOHIDVendorIDKey, vendorId, 32);
        multitouch_interface->setProperty(kIOHIDProductIDKey, vendorId, 32);

        setProperty(kIOHIDVendorIDKey, vendorId, 32);
        setProperty(kIOHIDProductIDKey, vendorId, 32);

        transducers = OSArray::withCapacity(numTransducers);
        if (!transducers) {
            IOLog("%s::No memory to allocate transducers array\n", getName());
            return;
        }
        DigitiserTransducerType type = kDigitiserTransducerFinger;
        for (int i = 0; i < numTransducers; i++) {
            VoodooI2CDigitiserTransducer* transducer = VoodooI2CDigitiserTransducer::transducer(type, NULL);
            transducer->type = kDigitiserTransducerFinger;

            transducer->logical_max_x = multitouch_interface->logical_max_x;
            transducer->logical_max_y = multitouch_interface->logical_max_y;
            transducer->id = i;
            transducer->secondary_id = i;

            transducers->setObject(transducer);
        }

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
        multitouch_interface->detach(this);
        OSSafeReleaseNULL(multitouch_interface);
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

IOFramebuffer* VoodooI2CGoodixEventDriver::getFramebuffer() {
    IODisplay* display = NULL;
    IOFramebuffer* framebuffer = NULL;

    OSDictionary *match = serviceMatching("IODisplay");
    OSIterator *iterator = getMatchingServices(match);

    if (iterator) {
        display = OSDynamicCast(IODisplay, iterator->getNextObject());

        if (display) {
            IOLog("%s::Got active display\n", getName());

            framebuffer = OSDynamicCast(IOFramebuffer, display->getParentEntry(gIOServicePlane)->getParentEntry(gIOServicePlane));

            if (framebuffer) {
                IOLog("%s::Got active framebuffer\n", getName());
            }
        }

        OSSafeReleaseNULL(iterator);
    }

    OSSafeReleaseNULL(match);

    return framebuffer;
}

void VoodooI2CGoodixEventDriver::checkRotation(IOFixed* x, IOFixed* y) {
    if (activeFramebuffer) {
        if (currentRotation & kIOFBSwapAxes) {
            IOFixed old_x = *x;
            *x = *y;
            *y = old_x;
        }
        if (currentRotation & kIOFBInvertX) {
            *x = 65535 - *x;
        }
        if (currentRotation & kIOFBInvertY) {
            *y = 65535 - *y;
        }
    }
}
