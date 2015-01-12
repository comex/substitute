typedef struct {
    bool modify;
} tdis_ret;
typedef struct tc {
    uintptr_t pc;
    int op_size;
    uint32_t op;
    uint32_t newop;
    uint32_t newval[4];
    uintptr_t pc_patch_start;
    uintptr_t pc_patch_end;
    bool got_bad;

} *tdis_ctx;

NOINLINE UNUSED
static tdis_ret P_data(struct tc *ctx, unsigned o0, unsigned o1, unsigned o2, unsigned o3, unsigned out_mask) {
    __builtin_abort();
}

NOINLINE UNUSED
static tdis_ret P_pcrel(struct tc *ctx, uintptr_t dpc, unsigned reg, bool is_load) {
    __builtin_abort();
}

NOINLINE UNUSED
static tdis_ret P_ret(struct tc *ctx) {
    /* ret is okay if it's at the end of the patch */
    if (ctx->pc + op_size < ctx->pc_patch_end) 
        ctx->got_bad = true;
    printf("ret: %08x\n", ctx->op);
    return (tdis_ret) {false};
}

NOINLINE UNUSED
static tdis_ret P_branch(struct tc *ctx, uintptr_t dpc) {
    if (dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end) {
        /* don't support this for now */
        ctx->got_bad = true;
    }
    return (tdis_ret) {false};
}

NOINLINE UNUSED
static tdis_ret P_unidentified(struct tc *ctx) {
    return (tdis_ret) {false};
}

NOINLINE UNUSED
static tdis_ret P_bad(struct tc *ctx) {
    ctx->got_bad = true;
    return (tdis_ret) {false};
}

#define P(x) transform_dis_##x
