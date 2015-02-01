#define JUMP_DIS_VERBOSE
#include <stdio.h>
#include "jump-dis.c"
#include <stdlib.h>
int main(UNUSED int argc, char **argv) {
    static char buf[1048576];
    UNUSED size_t size = fread(buf, 1, sizeof(buf), stdin);
    printf("size=%zd\n", size);
    int patch_size = atoi(argv[1]);
    struct arch_dis_ctx arch;
    arch_dis_ctx_init(&arch);
#ifdef TARGET_arm
    int thumb = atoi(argv[2]);
    arch.pc_low_bit = thumb;
#endif
    bool bad = P(main)(buf, 0x10000, 0x10000 + patch_size, arch);
    printf("final: bad = %d\n", bad);
}
