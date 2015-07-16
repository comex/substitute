#pragma once

#include "substitute-internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define INLINE __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))

static INLINE inline void unaligned_w64(void *ptr, uint64_t val) {
    __builtin_memcpy(ptr, &val, 8);
}
static INLINE inline void unaligned_w32(void *ptr, uint32_t val) {
    __builtin_memcpy(ptr, &val, 4);
}
static INLINE inline void unaligned_w16(void *ptr, uint16_t val) {
    __builtin_memcpy(ptr, &val, 2);
}
static INLINE inline uint64_t unaligned_r64(const void *ptr) {
    uint64_t val;
    __builtin_memcpy(&val, ptr, 8);
    return val;
}
static INLINE inline uint32_t unaligned_r32(const void *ptr) {
    uint32_t val;
    __builtin_memcpy(&val, ptr, 4);
    return val;
}
static INLINE inline uint16_t unaligned_r16(const void *ptr) {
    uint16_t val;
    __builtin_memcpy(&val, ptr, 2);
    return val;
}

struct bitslice_run {
    int inpos, outpos, len;
};

struct bitslice {
    int nruns;
    const struct bitslice_run *runs;
};

static inline int sext(unsigned val, int bits) {
    return val & (1 << (bits - 1)) ? ((int)val - (1 << bits)) : (int)val;
}

static inline unsigned bs_get(struct bitslice bs, unsigned op) {
    unsigned ret = 0;
    for(int i = 0; i < bs.nruns; i++) {
        const struct bitslice_run *run = &bs.runs[i];
        unsigned val = (op >> run->inpos) & ((1 << run->len) - 1);
        ret |= val << run->outpos;
    }
    return ret;
}

static inline unsigned bs_set(struct bitslice bs, unsigned new, unsigned op) {
    for(int i = 0; i < bs.nruns; i++) {
        const struct bitslice_run *run = &bs.runs[i];
        unsigned mask = (1 << run->len) - 1;
        unsigned val = (new >> run->outpos) & mask;
        op = (op & ~(mask << run->inpos)) | (val << run->inpos);
    }
    return op;
}

static inline struct bitslice bs_slice_(struct bitslice bs,
                                        struct bitslice_run *runs,
                                        int lo, int size) {
    int nruns = 0;
    for(int i = 0; i < bs.nruns; i++) {
        struct bitslice_run inr = bs.runs[i];
        inr.outpos -= lo;
        if(inr.outpos < 0) {
            inr.len += inr.outpos;
            inr.inpos -= inr.outpos;
            inr.outpos = 0;
        }
        if(inr.outpos + inr.len > size)
            inr.len = size - inr.outpos;
        if(inr.len > 0)
            runs[nruns++] = (struct bitslice_run) {inr.inpos, inr.outpos, inr.len};
    }
    return (struct bitslice) {nruns, runs};
}
#define bs_slice(bs, lo, size) \
    bs_slice_(bs, alloca((bs).nruns * sizeof(struct bitslice_run)), lo, size)

enum pcrel_load_mode {
    PLM_ADR, /* just want the address */
    PLM_U8,  PLM_S8,
    PLM_U16, PLM_S16,
    PLM_U32, PLM_S32,
    PLM_U64,
    PLM_U128, /* i.e. LDRD */
    PLM_U32_SIMD,
    PLM_U64_SIMD,
    PLM_U128_SIMD,
};

static const struct bitslice null_bs = { 0, NULL };
static const unsigned null_op = -0x100;
#define r(nn)           nn,                 false, true
#define rs(nn, l, s)    bs_slice(nn, l, s), false, true
#define rout(nn)        nn,                 true,  true
#define rsout(nn, l, s) bs_slice(nn, l, s), true,  true
#define rnull           null_bs,             false, false

#define data(...) data_flags(0, __VA_ARGS__)
#define data_flags(...) data_(__VA_ARGS__, rnull, rnull, rnull, rnull)
#define data_(...) data__(__VA_ARGS__)
#define data__(fl, b1, o1, v1, b2, o2, v2, b3, o3, v3, b4, o4, v4, ...) do { \
    unsigned op = ctx->base.op; \
    P(data)(ctx, \
        v1 ? bs_get(b1, op) : null_op, \
        v2 ? bs_get(b2, op) : null_op, \
        v3 ? bs_get(b3, op) : null_op, \
        v4 ? bs_get(b4, op) : null_op, \
        (o1 << 0) | \
        (o2 << 1) | \
        (o3 << 2) | \
        (o4 << 3) | \
        fl); \
    if (DIS_MAY_MODIFY && ctx->base.modify) { \
        uint32_t new = ctx->base.op; \
        new = bs_set(b1, ctx->base.newval[0], new); \
        new = bs_set(b2, ctx->base.newval[1], new); \
        new = bs_set(b3, ctx->base.newval[2], new); \
        new = bs_set(b4, ctx->base.newval[3], new); \
        *(uint32_t *) ctx->base.newop = new; \
    } \
    return; \
} while (0)

#ifndef TARGET_DIS_SUPPORTED
    #error "no disassembler for the target architecture yet"
#endif

static inline void op64(void **codep, uint64_t op) {
    unaligned_w64(*codep, op);
    *codep += 8;
}

static inline void op32(void **codep, uint32_t op) {
    unaligned_w32(*codep, op);
    *codep += 4;
}

static inline void op16(void **codep, uint16_t op) {
    unaligned_w16(*codep, op);
    *codep += 2;
}

static inline void op8(void **codep, uint8_t op) {
    *(uint8_t *) *codep = op;
    (*codep)++;
}

#define CC_CONDITIONAL 0x100
#define CC_CALL        0x200

struct dis_ctx_base {
    uint_tptr pc;
    const void *ptr;
#if defined(TARGET_x86_64) || defined(TARGET_i386)
    uint8_t newop[32];
#else
    uint8_t newop[4];
    uint32_t op;
#endif
    uint32_t newval[4];
    bool modify;
    int op_size, newop_size;
};

#include stringify(TARGET_DIR/arch-dis.h)
