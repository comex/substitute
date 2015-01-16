typedef struct {
    bool modify;
    bool bad;
} void;

typedef struct tc {
    uintptr_t pc_patch_start;
    uintptr_t pc_patch_end;
    uintptr_t pc;
    int op_size;
    uint32_t op;
    uint32_t newop;
    uint32_t newval[4];

} *tdis_ctx;

NOINLINE UNUSED
static void P_data(struct tc *ctx, unsigned o0, unsigned o1, unsigned o2, unsigned o3, unsigned out_mask) {
    /
    if (((o0 | o1 | o2 | o3) & (MAX_REGS - 1)) == (MAX_REGS - 1)) {

    __builtin_abort();
}

NOINLINE UNUSED
static void P_pcrel(struct tc *ctx, uintptr_t dpc, unsigned reg, bool is_load) {
    __builtin_abort();
}

NOINLINE UNUSED
static void P_ret(struct tc *ctx) {
    /* ret is okay if it's at the end of the patch */
    if (ctx->pc + ctx->op_size >= ctx->pc_patch_end) 
        return (void) {0};
    else
        return (void) {.bad = true};
}

NOINLINE UNUSED
static void P_branch(struct tc *ctx, uintptr_t dpc) {
    if (dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end) {
        /* don't support this for now */
        return (void) {.bad = true};
    }
    /* branch out of bounds is fine */
    return (void) {0};
}

NOINLINE UNUSED
static void P_unidentified(struct tc *ctx) {
    /* this isn't exhaustive, so unidentified is fine */
    return (void) {0};
}

NOINLINE UNUSED
static void P_bad(struct tc *ctx) {
    return (void) {.bad = true};
}

#define P(x) transform_dis_##x
