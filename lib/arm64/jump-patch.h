#pragma once
#include "arm64/assemble.h"
#define MAX_JUMP_PATCH_SIZE 12
#define MAX_REWRITTEN_SIZE (7 * 2 * 4) /* also conservative */
static inline int jump_patch_size(uintptr_t pc, uintptr_t dpc,
                                  struct arch_dis_ctx arch) {
    intptr_t diff = (dpc & ~0xfff) - (pc & ~0xfff);
    if (!(diff >= -0x100000000 && diff < 0x100000000))
        return -1;
    else if (pc & 0xfff)
        return 8;
    else
        return 12;
}

static inline void make_jump_patch(void **codep, uintptr_t pc, uintptr_t dpc,
                                   struct arch_dis_ctx arch) {
    ADRP_ADD(codep, 12 /* XXX */, pc, dpc);
}
