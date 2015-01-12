#pragma once

#include <stdbool.h>

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

static const struct bitslice nullbs = { 0, NULL };
#define r(nn)           nn,                 false, true
#define rs(nn, l, s)    bs_slice(nn, l, s), false, true
#define rout(nn)        nn,                 true,  true
#define rsout(nn, l, s) bs_slice(nn, l, s), true,  true
#define rnull           nullbs,             false, false
#define data(...) data_(__VA_ARGS__, rnull, rnull, rnull, rnull)
#define data_(...) data__(__VA_ARGS__)
#define data__(b1, o1, v1, b2, o2, v2, b3, o3, v3, b4, o4, v4, ...) \
    tdis_ret ret = P(data)(ctx, \
        v1 ? bs_get(b1, ctx->op) : -1u, \
        v2 ? bs_get(b2, ctx->op) : -1u, \
        v3 ? bs_get(b3, ctx->op) : -1u, \
        v4 ? bs_get(b4, ctx->op) : -1u, \
        (o1 << 0) | \
        (o2 << 1) | \
        (o3 << 2) | \
        (o4 << 3)); \
    if(ret.modify) { \
        unsigned new = ctx->op; \
        new = bs_set(b1, ctx->newval[0], new); \
        new = bs_set(b2, ctx->newval[1], new); \
        new = bs_set(b3, ctx->newval[2], new); \
        new = bs_set(b4, ctx->newval[3], new); \
        ctx->newop = new; \
    } \
    return ret;

