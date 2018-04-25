//
//  VoodooI2CAtmelMXT.c
//  VoodooI2CAtmelMXT
//
//  Created by CoolStar on 4/24/18.
//  Copyright © 2018 CoolStar. All rights reserved.
//

#include <mach/mach_types.h>

kern_return_t VoodooI2CAtmelMXT_start(kmod_info_t * ki, void *d);
kern_return_t VoodooI2CAtmelMXT_stop(kmod_info_t *ki, void *d);

kern_return_t VoodooI2CAtmelMXT_start(kmod_info_t * ki, void *d)
{
    return KERN_SUCCESS;
}

kern_return_t VoodooI2CAtmelMXT_stop(kmod_info_t *ki, void *d)
{
    return KERN_SUCCESS;
}
