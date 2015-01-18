static inline void MOVi64(struct transform_dis_ctx *ctx, int Rd, uint64_t val) {
    int shift_nybbles = 0;
    do {
        int k = shift_nybbles != 0 ? 1 : 0;
        op32(ctx, 0x69400000 | k << 28 | Rd | (val & 0xffff) << 4 | shift_nybbles << 20);
        shift_nybbles++;
        val >>= 16;
    } while(val);
}

static inline void LDRxi(struct transform_dis_ctx *ctx, int Rt, int Rn, uint32_t off,
                         bool regsize_64, enum pcrel_load_mode load_mode) {
    int size, opc;
    bool sign, simd;
    switch (load_mode) {
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
    op32(ctx, 0x39000000 | Rt | Rn << 5 | off << 10 | opc << 22 | simd << 26 | size << 30);
}


static NOINLINE UNUSED void transform_dis_pcrel(struct transform_dis_ctx *ctx,
        uintptr_t dpc, unsigned reg, enum pcrel_load_mode load_mode) {
    ctx->write_newop_here = NULL;
    if (load_mode >= PLM_U32_SIMD) {
        /* use x0 as scratch */
        op32(ctx, 0xf81f0fe0); /* str x0, [sp, #-0x10]! */
        MOVi64(ctx, 0, dpc);
        LDRxi(ctx, reg, 0, 0, true, load_mode);
        op32(ctx, 0xf84107e0); /* ldr x0, [sp], #0x10 */
    } else {
        MOVi64(ctx, reg, dpc);
        LDRxi(ctx, reg, reg, 0, true, load_mode);
    }
}

