#pragma once
#define MAX_JUMP_PATCH_SIZE 5
#include "dis.h"

static inline int jump_patch_size(uintptr_t pc, uintptr_t dpc,
                                  UNUSED struct arch_dis_ctx arch,
                                  bool force) {
    uintptr_t diff = pc - (dpc + 5);
    /* fits in 32? */
    if (diff == (uintptr_t) (int32_t) diff)
        return 5;
    else
        return force ? (2+4+8) : -1;
}

static inline void make_jump_patch(void **codep, uintptr_t pc, uintptr_t dpc,
                                   UNUSED struct arch_dis_ctx arch) {
    uintptr_t diff = pc - (dpc + 5);
    void *code = *codep;
    if (diff == (uintptr_t) (int32_t) diff) {
        op8(&code, 0xe9);
        op32(&code, diff);
    } else {
        /* jmpq *(%rip) */
        op8(&code, 0xff);
        op8(&code, 0x25);
        op32(&code, 0);
        op64(&code, dpc);
    }
    *codep = code;
}
