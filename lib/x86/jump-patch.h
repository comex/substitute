#pragma once
#define MAX_JUMP_PATCH_SIZE 14
#define MAX_EXTENDED_PATCH_SIZE (MAX_JUMP_PATCH_SIZE+14)
#include "dis.h"

static inline int jump_patch_size(uint_tptr pc, uint_tptr dpc,
                                  UNUSED struct arch_dis_ctx arch,
                                  bool force) {
    uint_tptr diff = dpc - (pc + 5);
    /* fits in 32? */
    if (diff == (uint_tptr) (int32_t) diff)
        return 5;
    else
        return force ? (2+4+8) : -1;
}

static inline void make_jmp_or_call(void **codep, uint_tptr pc, uint_tptr dpc,
                                    bool call) {
    uint_tptr diff = dpc - (pc + 5);
    void *code = *codep;
    if (diff == (uint_tptr) (int32_t) diff) {
        op8(&code, call ? 0xe8 : 0xe9);
        op32(&code, diff);
    } else {
        /* jmpq *(%rip) */
        op8(&code, 0xff);
        op8(&code, call ? 0x15 : 0x25);
        op32(&code, 0);
        op64(&code, dpc);
    }
    *codep = code;
}

static inline void make_jump_patch(void **codep, uint_tptr pc, uint_tptr dpc,
                                   UNUSED struct arch_dis_ctx arch) {
    make_jmp_or_call(codep, pc, dpc, false);
}
