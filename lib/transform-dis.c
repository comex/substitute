#include "substitute-internal.h"
#ifdef TARGET_DIS_SUPPORTED
#define DIS_MAY_MODIFY 1

#include "substitute.h"
#include "dis.h"
#include "transform-dis.h"
#include <stdbool.h>
#include <stdint.h>

#define P(x) transform_dis_##x

struct transform_dis_ctx {
    /* outputs */
    int err;
    struct dis_ctx_base base;

    uint_tptr pc_trampoline;
    uint_tptr pc_patch_start;
    /* this is only tentative - it will be updated to include parts of
     * instructions poking out, and instructions forced to be transformed by IT */
    uint_tptr pc_patch_end;
    /* for IT - eww */
    bool force_keep_transforming;

    bool ban_calls; /* i.e. trying to be thread safe */
    bool ban_jumps; /* allow transforming rel branches at beginning */

    void **rewritten_ptr_ptr;
    void *write_newop_here;

    struct arch_dis_ctx arch;
};

#define tdis_ctx struct transform_dis_ctx *

/* largely similar to jump_dis */

static NOINLINE UNUSED
void transform_dis_indirect_call(struct transform_dis_ctx *ctx) {
    /* see error description */
    if (ctx->ban_calls && ctx->base.pc + ctx->base.op_size < ctx->pc_patch_end)
        ctx->err = SUBSTITUTE_ERR_FUNC_CALLS_AT_START;
}

static NOINLINE UNUSED
void transform_dis_ret(struct transform_dis_ctx *ctx) {
    /* ret is okay if it's at the end of the required patch (past the original
     * patch size is good too) */
    if (ctx->base.pc + ctx->base.op_size < ctx->pc_patch_end)
        ctx->err = SUBSTITUTE_ERR_FUNC_TOO_SHORT;
}

static UNUSED
void transform_dis_unidentified(UNUSED struct transform_dis_ctx *ctx) {
#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis (0x%llx): unidentified\n", 
           (unsigned long long) ctx->base.pc);
#endif
    /* this isn't exhaustive, so unidentified is fine */
}

static NOINLINE UNUSED
void transform_dis_bad(struct transform_dis_ctx *ctx) {
    ctx->err = SUBSTITUTE_ERR_FUNC_BAD_INSN_AT_START;
}

static INLINE UNUSED
void transform_dis_thumb_it(UNUSED struct transform_dis_ctx *ctx) {
    /* ignore, since it was turned into B */
}

static void transform_dis_branch_top(struct transform_dis_ctx *ctx,
                                     uintptr_t dpc, int cc) {
    if (dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end) {
        /* don't support this for now */
        ctx->err = SUBSTITUTE_ERR_FUNC_BAD_INSN_AT_START;
        return;
    }
    if (cc & CC_CALL) {
        transform_dis_indirect_call(ctx);
    } else if (ctx->ban_jumps) {
        transform_dis_ret(ctx);
    }
}

static void transform_dis_dis(struct transform_dis_ctx *ctx);
static void transform_dis_pre_dis(struct transform_dis_ctx *ctx);
static void transform_dis_post_dis(struct transform_dis_ctx *ctx);

int transform_dis_main(const void *restrict code_ptr,
                       void **restrict rewritten_ptr_ptr,
                       uint_tptr pc_patch_start,
                       uint_tptr *pc_patch_end_p,
                       uint_tptr pc_trampoline,
                       struct arch_dis_ctx *arch_ctx_p,
                       int *offset_by_pcdiff,
                       int options) {
    struct transform_dis_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.pc_patch_start = pc_patch_start;
    ctx.pc_patch_end = *pc_patch_end_p;
    ctx.base.pc = pc_patch_start;
    ctx.arch = *arch_ctx_p;
    ctx.ban_calls = options & TRANSFORM_DIS_BAN_CALLS;
    ctx.ban_jumps = options & TRANSFORM_DIS_REL_JUMPS;
    /* data is written to rewritten both by this function directly and, in case
     * additional scaffolding is needed, by arch-specific transform_dis_* */
    ctx.rewritten_ptr_ptr = rewritten_ptr_ptr;
    void *rewritten_start = *rewritten_ptr_ptr;
    int written_pcdiff = 0;
    offset_by_pcdiff[written_pcdiff++] = 0;
    while (ctx.base.pc < ctx.pc_patch_end && !ctx.force_keep_transforming) {
        ctx.base.modify = false;
        ctx.err = 0;
        ctx.base.ptr = code_ptr + (ctx.base.pc - pc_patch_start);
        ctx.pc_trampoline = pc_trampoline +
                            (*rewritten_ptr_ptr - rewritten_start);
        const void *start = ctx.base.ptr;

        transform_dis_pre_dis(&ctx);

        void *rewritten_ptr = *rewritten_ptr_ptr;
        ctx.write_newop_here = rewritten_ptr;

        transform_dis_dis(&ctx);

#ifdef TRANSFORM_DIS_VERBOSE
        printf("transform_dis (0x%llx): >> op_size=%d newop_size=%d\n", 
               (unsigned long long) ctx.base.pc,
               ctx.base.op_size,
               ctx.base.newop_size);
#endif

        if (ctx.err)
            return ctx.err;
        if (ctx.write_newop_here != NULL) {
            if (ctx.base.modify)
                memcpy(ctx.write_newop_here, ctx.base.newop, ctx.base.newop_size);
            else
                memcpy(ctx.write_newop_here, start, ctx.base.op_size);
            if (*rewritten_ptr_ptr == rewritten_ptr)
                *rewritten_ptr_ptr += ctx.base.op_size;
        }
        ctx.base.pc += ctx.base.op_size;

        transform_dis_post_dis(&ctx);

        int pcdiff = ctx.base.pc - ctx.pc_patch_start;
        while (written_pcdiff < pcdiff)
            offset_by_pcdiff[written_pcdiff++] = -1;
        offset_by_pcdiff[written_pcdiff++] =
                (int) (*rewritten_ptr_ptr - rewritten_start);
    }
    *pc_patch_end_p = ctx.base.pc;
    *arch_ctx_p = ctx.arch;
    return SUBSTITUTE_OK;
}

#include stringify(TARGET_DIR/arch-transform-dis.inc.h)
#include stringify(TARGET_DIR/dis-main.inc.h)

#endif /* TARGET_DIS_SUPPORTED */
