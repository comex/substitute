#include "arm64/assemble.h"

static NOINLINE UNUSED void transform_dis_pcrel(struct transform_dis_ctx *ctx,
        uintptr_t dpc, unsigned reg, enum pcrel_load_mode load_mode) {
    ctx->write_newop_here = NULL;
    void **codep = ctx->rewritten_ptr_ptr;
    if (load_mode >= PLM_U32_SIMD) {
        /* use x0 as scratch */
        op32(codep, 0xf81f0fe0); /* str x0, [sp, #-0x10]! */
        MOVi64(codep, 0, dpc);
        LDRxi(codep, reg, 0, 0, true, load_mode);
        op32(codep, 0xf84107e0); /* ldr x0, [sp], #0x10 */
    } else {
        MOVi64(codep, reg, dpc);
        LDRxi(codep, reg, reg, 0, true, load_mode);
    }
}

static NOINLINE UNUSED void transform_dis_branch(struct transform_dis_ctx *ctx,
        uintptr_t dpc, int cc) {
#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis (%p): branch => %p\n", (void *) ctx->pc, (void *) dpc);
#endif
    if (dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end) {
        ctx->err = SUBSTITUTE_ERR_FUNC_BAD_INSN_AT_START;
        return;
    }
    /* TODO */
    (void) cc;
}

static void transform_dis_pre_dis(UNUSED struct transform_dis_ctx *ctx) {}
static void transform_dis_post_dis(UNUSED struct transform_dis_ctx *ctx) {}
