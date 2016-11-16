#include "transform-dis-cases.h"
#ifdef THUMB
.thumb
#endif


#ifndef THUMB
GIVEN blne 0f; nop; nop; 0:
EXPECT beq 1f; movw lr, #0x000c; movt lr, #0xdead; blx lr; 1: nop; nop
GIVEN bl 0f; 0:
EXPECT movw lr, #0x0004; movt lr, #0xdead; blx lr
#else
GIVEN bl 0f; 0:
EXPECT movw lr, #0x0005; movt lr, #0xdead; blx lr
#endif
