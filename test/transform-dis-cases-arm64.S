#include "transform-dis-cases.h"

/* yay clang, no semicolons allowed */

GIVEN
    blr x5
EXPECT
    blr x5

GIVEN
    blr x5
    nop
//EXPECT_ERR (with ban_calls)
EXPECT
    blr x5


GIVEN
    cbnz x8, .+0x100
EXPECT
    cbz x8, 1f
    mov  x18, #0x0100
    movk x18, #0xdead, lsl #16
    br x18
    1:

GIVEN
    0: mov x18, #0x123
    cbnz x18, 0b+0x100
EXPECT
    mov x18, #0x123
    cbz x18, 1f
    mov  x17, #0x0100
    movk x17, #0xdead, lsl #16
    br x17
    1:
