#pragma once
#include <stdbool.h>
#include <stdint.h>

#define UNUSED __attribute__((unused))
#define INLINE __attribute__((always_inline)) inline 
#ifdef __cplusplus
#define CONSTEXPR constexpr
#else
#define CONSTEXPR
#endif

struct bitslice_run {
    int8_t inpos, outpos, len;
};

struct bitslice {
    int8_t nruns;
    struct bitslice_run runs[6];
};

struct dis_data_operand {
    bool out;
    const struct bitslice *n;
};

static inline CONSTEXPR int sext(unsigned val, int bits) {
    return val & (1 << (bits - 1)) ? ((int)val - (1 << bits)) : (int)val;
}

static inline CONSTEXPR unsigned bs_get(const struct bitslice *bs, unsigned op) {
    unsigned ret = 0;
    for(int i = 0; i < bs->nruns; i++) {
        const struct bitslice_run *run = &bs->runs[i];
        unsigned val = (op >> run->inpos) & ((1 << run->len) - 1);
        ret |= val << run->outpos;
    }
    return ret;
}

static inline CONSTEXPR unsigned bs_set(const struct bitslice *bs, unsigned new_, unsigned op) {
    for(int i = 0; i < bs->nruns; i++) {
        const struct bitslice_run *run = &bs->runs[i];
        unsigned mask = (1 << run->len) - 1;
        unsigned val = (new_ >> run->outpos) & mask;
        op = (op & ~(mask << run->inpos)) | (val << run->inpos);
    }
    return op;
}

static inline CONSTEXPR struct bitslice bs_slice(const struct bitslice *bs, int lo, int size) {
    struct bitslice obs
    #ifdef __cplusplus
    {}
    #endif
    ;
    obs.nruns = 0;
    for(int i = 0; i < bs->nruns; i++) {
        struct bitslice_run inr = bs->runs[i];
        inr.outpos -= lo;
        if(inr.outpos < 0) {
            inr.len += inr.outpos;
            inr.inpos -= inr.outpos;
            inr.outpos = 0;
        }
        if(inr.outpos + inr.len > size)
            inr.len = size - inr.outpos;
        if(inr.len > 0)
            obs.runs[obs.nruns++] = inr;
    }
    return obs;
}


#ifdef __cplusplus
#define staticify(ty, ...) [&](){ constexpr static ty bs = __VA_ARGS__; return &bs; }()

#define r(nn)           staticify(struct dis_data_operand, {.n = nn, .out = false})
#define rs(nn, l, s)    staticify(struct dis_data_operand, {.n = staticify(struct bitslice, bs_slice(nn, l, s)), .out = false})
#define rout(nn)        staticify(struct dis_data_operand, {.n = nn, .out = true})
#define rsout(nn, l, s) staticify(struct dis_data_operand, {.n = staticify(struct bitslice, bs_slice(nn, l, s)), .out = true})

#define data(...) return P(data)<__VA_ARGS__>(ctx);
typedef const struct bitslice *BSP;

#endif
