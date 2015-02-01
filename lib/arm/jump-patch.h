#pragma once
#include "dis.h"
#include "arm/assemble.h"
#define MAX_JUMP_PATCH_SIZE 8

static inline int jump_patch_size(UNUSED uintptr_t pc,
                                  UNUSED uintptr_t dpc,
                                  UNUSED struct arch_dis_ctx arch,
                                  UNUSED bool force) {
    return 8;
}

static inline void make_jump_patch(void **codep, UNUSED uintptr_t pc,
                                   uintptr_t dpc,
                                   struct arch_dis_ctx arch) {
    struct assemble_ctx actx = {codep, arch.pc_low_bit, 0xe};
    LDR_PC(actx, dpc);
}
