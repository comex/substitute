static void P(dis_onebyte)(tdis_ctx ctx) {
    uint8_t *ptr = ctx->ptr;
    uint8_t byte1 = *ptr++;
    uint8_t bits = onebyte_bits[byte1];
    uint8_t byte2 = 0;
    if (byte1 == 0x0f) {
        byte2 = *ptr++;
        bits = _0f_bits[byte2];
        if (byte2 == 0x39) {
            XXX
        } else if (byte2 == 0x3b) {
            XXX
        }
    } else if ((byte1 & 0xd8) == 0xd8) {
        *ptr++;
        bits = I_MODA;
    }
    // get modrm
    int mod, rm, mrlow;
    if (bits & I_MOD) {
        uint8_t modrm = *ptr++;
        mod = modrm >> 6;
        rm = modrm >> 3 & 7;
        mrlow = modrm & 7;
        if (rm == 4) {
            /* sib */
            ptr++;
        }
    }
    if (bits & I_PFX) {
        // this could affect opcode size etc... then we restart

    }
};

static void P(dis_x86)(tdis_ctx ctx) {
    void *orig = ctx->ptr;
    P(dis_onebyte)(ctx);
    ctx->op_size = ctx->ptr - orig;
}
