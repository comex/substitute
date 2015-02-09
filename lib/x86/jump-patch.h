#pragma once
#define MAX_JUMP_PATCH_SIZE 5

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

static inline void make_jump_patch(void **codep, UNUSED uintptr_t pc,
                                   uintptr_t dpc,
                                   UNUSED struct arch_dis_ctx arch) {
    uintptr_t diff = pc - (dpc + 5);
    uint8_t *code = *codep;
    if (diff == (uintptr_t) (int32_t) diff) {
        *(uint8_t *) code = 0xe9;
        *(uint32_t *) (code + 1) = diff;
        *codep = code + 5;
    } else {
        /* jmpq *(%rip) */
        *code++ = 0xff;
        *code++ = 0x25;
        *(uint32_t *) code = 0; code += 4;
        *(uint64_t *) code = dpc; code += 8;
        *codep = code;
    }
}
