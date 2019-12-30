//
//  VoodooI2CGoodixTouchDriver.hpp
//  VoodooI2CGoodix
//
//  Created by CoolStar on 4/24/18.
//  Copyright © 2018 CoolStar. All rights reserved.
//

#ifndef VoodooI2CGoodixTouchDriver_hpp
#define VoodooI2CGoodixTouchDriver_hpp

#include <IOKit/IOService.h>

class VoodooI2CGoodixTouchDriver : public IOService {
    OSDeclareDefaultStructors(VoodooI2CGoodixTouchDriver);

public:
    /* Initialises the VoodooI2CGoodixTouchDriver object/instance (intended as IOKit driver ctor)
     *
     * @return true if properly initialised
     */
    bool init(OSDictionary* properties = 0) override;
    /* Frees any allocated resources, called implicitly by the kernel
     * as the last stage of the driver being unloaded
     *
     */
    void free() override;
    /* Checks if an Goodix device exists on the current system
     *
     * @return returns an instance of the current VoodooI2CGoodixTouchDriver if there is a matched Goodix device, NULL otherwise
     */
    IOService *probe(IOService* provider, SInt32* score) override;
    /* Starts the driver and initialises the Goodix device
     *
     * @return returns true if the driver has started
     */
    bool start(IOService* provider) override;
    /* Stops the driver and frees any allocated resource
     *
     */
    void stop(IOService* provider) override;
    
protected:
    IOReturn setPowerState(unsigned long powerState, IOService* whatDevice) override;

};

#endif /* VoodooI2CGoodixTouchDriver_hpp */
