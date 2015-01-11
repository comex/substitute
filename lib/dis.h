#pragma once

static inline int sext(unsigned val, int bits) {
    return val & (1 << (bits - 1)) ? ((int)val - (1 << bits)) : (int)val;
}

struct bitslice_run {
    int inpos, outpos, len;
};

struct bitslice {
    int nruns;
    const struct bitslice_run *runs;
};

__attribute__((always_inline))
static inline unsigned bs_get(struct bitslice bs, unsigned op) {
    unsigned ret = 0;
    for(int i = 0; i < bs.nruns; i++) {
        const struct bitslice_run *run = &bs.runs[i];
        unsigned masked = op & ((1 << run->len) - 1);
        if (run->outpos < run->inpos)
            masked >>= run->inpos - run->outpos;
        else if (run->outpos > run->inpos)
            masked <<= run->outpos - run->inpos;
        ret |= masked;
    }
    return ret;
}

__attribute__((always_inline))
static inline unsigned bs_set(struct bitslice bs, unsigned val, unsigned op) {
    for(int i = 0; i < bs.nruns; i++) {
        const struct bitslice_run *run = &bs.runs[i];
        unsigned mask = (1 << run->len) - 1;
        unsigned masked = val & mask;
        if (run->outpos < run->inpos) {
            masked <<= run->inpos - run->outpos;
            mask <<= run->inpos - run->outpos;
        } else if (run->outpos > run->inpos) {
            masked >>= run->outpos - run->inpos;
            mask >>= run->outpos - run->inpos;
        }
        op = (op & ~mask) | masked;
    }
    return op;
}
