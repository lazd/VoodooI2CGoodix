//
//  MyIOFramebuffer.cpp
//  VoodooI2CGoodix
//
//  Created by jason on 2020/12/26.
//  Copyright © 2020 lazd. All rights reserved.
//


#include "MyIOFramebuffer.hpp"

#define GetShmem(instance)      ((StdFBShmem_t *)(instance->priv))

void MyIOFramebuffer::MyHideCursor()
{
    hideCursor();
}

void MyIOFramebuffer::MyShowCursor()
{
    StdFBShmem_t *shmem;
    shmem = GetShmem(this);
    showCursor( &shmem->cursorLoc, shmem->frame );
}
