//
//  MyIOFramebuffer.hpp
//  VoodooI2CGoodix
//
//  Created by jason on 2020/12/26.
//  Copyright © 2020 lazd. All rights reserved.
//

#ifndef MyIOFramebuffer_hpp
#define MyIOFramebuffer_hpp

#include <IOKit/graphics/IOFramebuffer.h>

class MyIOFramebuffer : public IOFramebuffer {
public:
    void MyHideCursor();
    void MyShowCursor();
};

#endif /* MyIOFramebuffer_hpp */
