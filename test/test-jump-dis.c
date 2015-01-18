#define JUMP_DIS_VERBOSE
#include <stdio.h>
#include "jump-dis.c"
#include <stdlib.h>
int main(UNUSED int argc, char **argv) {
    static char buf[1048576];
    UNUSED size_t size = fread(buf, 1, sizeof(buf), stdin);
    printf("size=%zd\n", size);
    int patch_size = atoi(argv[1]);
    int thumb = atoi(argv[2]);
    bool bad = P(main)(buf, 0x10000, 0x10000 + patch_size, thumb);
    printf("final: bad = %d\n", bad);
}
