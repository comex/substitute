#define GIVEN .ascii "GIVEN";
#define EXPECT .ascii "EXPECT";
#define EXPECT_ERR .ascii "EXPECT_ERR";

GIVEN call .; pop %edx
/* XXX the extra push isn't necessary in 32-bit mode */
EXPECT push %eax; push %eax; mov $0xdead0005, %eax; pop %eax; pop %edx

GIVEN jmp 0f; 0:
EXPECT_ERR

GIVEN jne 0xdead1000
EXPECT jne 0f; jmp 1f; 0: jmp 0xdead1000; 1:

GIVEN loopne 0xdead0080
EXPECT loopne 0f; jmp 1f; 0: jmp 0xdead1000; 1:
