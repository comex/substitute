#define GIVEN .ascii "GIVEN";
#define EXPECT .ascii "EXPECT";
#define EXPECT_ERR .ascii "EXPECT_ERR";

GIVEN nopl (%rip)
EXPECT push %rcx; mov $0xdead0007, %rcx; nopl (%rcx); pop %rcx
GIVEN mov (%rip), %rax
EXPECT push %rcx; mov $0xdead0007, %rcx; mov (%rcx), %rax; pop %rcx
GIVEN mov (%rip), %rcx
EXPECT push %rax; mov $0xdead0007, %rax; mov (%rax), %rcx; pop %rax
GIVEN jmpq *(%rip)
EXPECT push %rax; mov $0xdead0006, %rax; mov %rax, -8(%rsp); pop %rax;
       jmp *-0x10(%rsp)
