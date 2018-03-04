#pragma once
#include "dis.h"
#include "arm/assemble.h"
#define MAX_JUMP_PATCH_SIZE 8
#define MAX_EXTENDED_PATCH_SIZE (MAX_JUMP_PATCH_SIZE+14)

static inline int jump_patch_size(uint_tptr pc,
                                  UNUSED uint_tptr dpc,
                                  UNUSED struct arch_dis_ctx arch,
                                  UNUSED bool force) {
    return (pc & 2) ? 10 : 8;
}

static inline void make_jump_patch(void **codep, uint_tptr pc,
                                   uint_tptr dpc,
                                   struct arch_dis_ctx arch) {
    struct assemble_ctx actx = {codep, (void *)pc, arch.pc_low_bit, 0xe};
    LDR_PC(actx, dpc);
}
