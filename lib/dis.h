#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define UNUSED __attribute__((unused))
#define INLINE __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))

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

static inline struct bitslice bs_slice_(struct bitslice bs, struct bitslice_run *runs, int lo, int size) {
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
    PLM_U128,
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
#define data(...) data_(__VA_ARGS__, rnull, rnull, rnull, rnull)
#define data_(...) data__(__VA_ARGS__)
#define data__(b1, o1, v1, b2, o2, v2, b3, o3, v3, b4, o4, v4, ...) do { \
    P(data)(ctx, \
        v1 ? bs_get(b1, ctx->op) : null_op, \
        v2 ? bs_get(b2, ctx->op) : null_op, \
        v3 ? bs_get(b3, ctx->op) : null_op, \
        v4 ? bs_get(b4, ctx->op) : null_op, \
        (o1 << 0) | \
        (o2 << 1) | \
        (o3 << 2) | \
        (o4 << 3)); \
    if (TDIS_CTX_MODIFY(ctx)) { \
        unsigned new = ctx->op; \
        new = bs_set(b1, TDIS_CTX_NEWVAL(ctx, 0), new); \
        new = bs_set(b2, TDIS_CTX_NEWVAL(ctx, 1), new); \
        new = bs_set(b3, TDIS_CTX_NEWVAL(ctx, 2), new); \
        new = bs_set(b4, TDIS_CTX_NEWVAL(ctx, 3), new); \
        TDIS_CTX_SET_NEWOP(ctx, new); \
    } \
    return; \
} while (0)

