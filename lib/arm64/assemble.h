#pragma once
#include "dis.h"

static inline int size_of_MOVi64(uint64_t val) {
    int num_nybbles = val == 0 ? 1 : ((64 - __builtin_clzll(val) + 15) / 16);
    return 4 * num_nybbles;
}

static inline void MOVi64(void **codep, int Rd, uint64_t val) {
    int shift_nybbles = 0;
    do {
        int k = shift_nybbles != 0;
        op32(codep, 0xd2800000 | k << 29 | Rd | (val & 0xffff) << 5 |
                    shift_nybbles << 21);
        shift_nybbles++;
        val >>= 16;
    } while(val);
}

static inline void LDRxi(void **codep, int Rt, int Rn, uint32_t off,
                         bool regsize_64, enum pcrel_load_mode load_mode) {
    int size, opc;
    bool sign, simd;
    switch (load_mode) {
        case PLM_ADR: return;
        case PLM_U8:  size = 0; sign = false;  simd = false; break;
        case PLM_S8:  size = 0; sign = true;   simd = false; break;
        case PLM_U16: size = 1; sign = false;  simd = false; break;
        case PLM_S16: size = 1; sign = true;   simd = false; break;
        case PLM_U32: size = 2; sign = false;  simd = false; break;
        case PLM_S32: size = 2; sign = true;   simd = false; break;
        case PLM_U64: size = 3; sign = false;  simd = false; break;
        case PLM_U32_SIMD:  size = 2; opc = 1; simd = true; break;
        case PLM_U64_SIMD:  size = 3; opc = 1; simd = true; break;
        case PLM_U128_SIMD: size = 0; opc = 3; simd = true; break;
        default: __builtin_abort();
    }
    if (simd) {
        off /= 1 << (size | (opc & 1) << 2);
    } else {
        off /= 1 << size;
        opc = sign ? (regsize_64 ? 2 : 3) : 1;
    }
    op32(codep, 0x39000000 | Rt | Rn << 5 | off << 10 | opc << 22 | simd << 26 |
                size << 30);
}

static inline void ADRP_ADD(void **codep, int reg, uint64_t pc, uint64_t dpc) {
    uint64_t diff = (dpc & ~0xfff) - (pc & ~0xfff);
    /* ADRP reg, dpc */
    op32(codep, 0x90000000 | reg | (diff & 0x3000) << 17 |
                (diff & 0x1ffffc000) >> 9);
    uint32_t lo = dpc & 0xfff;
    if (lo) {
        /* ADD reg, reg, #lo */
        op32(codep, 0x91000000 | reg | reg << 5 | lo << 10);
    }
}

static inline void BR(void **codep, int reg, bool link) {
    op32(codep, 0xd61f0000 | reg << 5 | link << 21);
}

static inline void Bccrel(void **codep, int cc, int offset) {
    op32(codep, 0x54000000 | (offset / 4) << 5 | cc);
}

