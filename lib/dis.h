#pragma once
#include <stdbool.h>

#define UNUSED __attribute__((unused))
#define INLINE inline __attribute__((always_inline))

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

struct operand_internal {
    struct bitslice n;
    bool out;
    bool valid;
};

#define r(nn)           {.n = nn, .out = false, .valid = true}
#define rs(nn, l, s)    {.n = bs_slice(nn, l, s), .out = false, .valid = true}
#define rout(nn)        {.n = nn, .out = true, .valid = true}
#define rsout(nn, l, s) {.n = bs_slice(nn, l, s), .out = true, .valid = true}
#define data(...) \
    struct operand_internal ops[4] = {__VA_ARGS__}; \
    tdis_ret ret = P(data)(ctx, \
        ops[0].valid ? bs_get(ops[0].n, ctx->op) : -1u, \
        ops[1].valid ? bs_get(ops[1].n, ctx->op) : -1u, \
        ops[2].valid ? bs_get(ops[2].n, ctx->op) : -1u, \
        ops[3].valid ? bs_get(ops[3].n, ctx->op) : -1u, \
        (ops[0].valid << 0) | \
        (ops[1].valid << 1) | \
        (ops[2].valid << 2) | \
        (ops[3].valid << 3)); \
    if(ret.modify) { \
        unsigned new = ctx->op; \
        new = bs_set(ops[0].n, ctx->newval[0], new); \
        new = bs_set(ops[1].n, ctx->newval[1], new); \
        new = bs_set(ops[2].n, ctx->newval[2], new); \
        new = bs_set(ops[3].n, ctx->newval[3], new); \
        ctx->newop = new; \
    } \
    return ret;

