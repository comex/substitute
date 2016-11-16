#include "transform-dis-cases.h"

GIVEN call 0f; 0: pop %edx
/* XXX the extra push isn't necessary in 32-bit mode */
EXPECT push %eax; push %eax; mov $0xdead0005, %eax; mov %eax, 4(%esp); pop %eax; pop %edx

GIVEN jmp 0f; 0: nop
EXPECT_ERR

GIVEN jne .+0x1000
/* we expect to generate an unnecessarily long jump, so hardcode it
 * the 0x10000 is because we pretend our trampoline is 0x10000 bytes before the
 * original function */
EXPECT 2: .byte 0x0f, 0x85; .long 2; jmp 1f; 0: jmp .+0x10000+0x1000-8; 1:

GIVEN loopne .+0x80
EXPECT loopne 0f; jmp 1f; 0: jmp .+0x10000+0x80-4; 1:

GIVEN call .+0x1000; nop
EXPECT_ERR

GIVEN call *%edi
EXPECT call *%edi

GIVEN call *%edi; nop
EXPECT_ERR

GIVEN call *(%edi); nop
EXPECT_ERR
