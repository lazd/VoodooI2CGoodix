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
    /*
    // set interrupts AFTER device is initialised
    interrupt_source = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &VoodooI2CGoodixTouchDriver::interrupt_occurred), api, 0);
    if (!interrupt_source) {
        IOLog("%s::Could not get interrupt event source\n", getName());
        goto start_exit;
    }
    */
    publish_multitouch_interface();
     if (!init_device()) {
        IOLog("%s::Failed to init device\n", getName());
        return NULL;
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

void VoodooI2CGoodixTouchDriver::stop(IOService* provider) {
    release_resources();
    unpublish_multitouch_interface();
    PMstop();
    IOLog("%s::Stopped\n", getName());
    super::stop(provider);
}

IOReturn VoodooI2CGoodixTouchDriver::setPowerState(unsigned long powerState, IOService* whatDevice) {
    if (whatDevice != this) {
        return kIOReturnInvalid;
    }

    return kIOPMAckImplied;
}

bool VoodooI2CGoodixTouchDriver::init_device() {
    return true;
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
