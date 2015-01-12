#pragma once
#include <stdbool.h>

#define UNUSED __attribute__((unused))

struct bitslice_run {
    int inpos, outpos, len;
};

struct bitslice {
    int nruns;
    const struct bitslice_run *runs;
};

struct dis_data_operand {
    struct bitslice n;
    bool out;
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

