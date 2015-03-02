#pragma once
#include "arm64/assemble.h"
#define MAX_JUMP_PATCH_SIZE 20
#define MAX_EXTENDED_PATCH_SIZE MAX_JUMP_PATCH_SIZE

static inline int jump_patch_size(uint_tptr pc, uint_tptr dpc,
                                  UNUSED struct arch_dis_ctx arch,
                                  bool force) {
    intptr_t diff = (dpc & ~0xfff) - (pc & ~0xfff);
    if (!(diff >= -0x100000000 && diff < 0x100000000))
        return force ? (size_of_MOVi64(dpc) + 4) : -1;
    else if (!(dpc & 0xfff))
        return 8;
    else
        return 12;
}

static inline void make_jump_patch(void **codep, uint_tptr pc, uint_tptr dpc,
                                   struct arch_dis_ctx arch) {
    int reg = arm64_get_unwritten_temp_reg(&arch);
    intptr_t diff = (dpc & ~0xfff) - (pc & ~0xfff);
    if (!(diff >= -0x100000000 && diff < 0x100000000))
        MOVi64(codep, reg, dpc);
    else
        ADRP_ADD(codep, reg, pc, dpc);
    BR(codep, reg, false);
}
