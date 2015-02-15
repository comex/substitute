#define GIVEN .ascii "GIVEN";
#define EXPECT .ascii "EXPECT";
#define EXPECT_ERR .ascii "EXPECT_ERR";

GIVEN call 0f; 0: pop %edx
/* XXX the extra push isn't necessary in 32-bit mode */
EXPECT push %eax; push %eax; mov $0xdead0005, %eax; mov %eax, 4(%esp); pop %eax; pop %edx

GIVEN jmp 0f; 0: nop
EXPECT_ERR

GIVEN jne .+0x1000
/* we expect to generate an unnecessarily long jump, so hardcode it */
EXPECT 2: .byte 0x0f, 0x85; .long 2; jmp 1f; 0: jmp 2b+0x1000; 1:

GIVEN loopne 0xdead0080
EXPECT loopne 0f; jmp 1f; 0: jmp 0xdead1000; 1:
