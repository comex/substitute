#include "cbit/vec.h"
#include "substitute-internal.h"
#ifdef TARGET_DIS_SUPPORTED
#define DIS_MAY_MODIFY 0
#include "dis.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* This pass tries to look through the function to find jumps back to the
 * patched code at the beginning to the function.  It does not deal with jump
 * tables, and has a limited range, so it is only heuristic.  If such jumps are
 * found, the hook is aborted.  In the future it might be possible to fix up
 * the jumps rather than merely detect them, but that would require doing
 * something weird like extending the patch region to add trampolines... */

enum {
    JUMP_ANALYSIS_MAX_INSNS = 512,
    JUMP_ANALYSIS_MAX_SIZE = JUMP_ANALYSIS_MAX_INSNS * MIN_INSN_SIZE,
};

DECL_VEC(uint_tptr, uint_tptr);

struct jump_dis_ctx {
    /* outputs */
    bool bad_insn;
    bool continue_after_this_insn;

    struct dis_ctx_base base;

    uint_tptr pc_patch_start;
    uint_tptr pc_patch_end;
    /* For now, this is pretty hacky.  Once we find a ret, we don't process any
     * instructions after it, because they might be calls to __stack_chk_fail.
     * That means any function with a ret midway will only have part of it
     * checked, but whatever; heuristic, remember.  Instructions are not
     * actually guaranteed to be processed in sorted order, but we'll follow
     * the straight line before branches, which should be good enough. */
    uint_tptr pc_ret;

    uint8_t seen_mask[JUMP_ANALYSIS_MAX_INSNS / 8];
    /* queue of instructions to visit (well, stack) */
    VEC_STORAGE_CAPA(uint_tptr, 10) queue;

    struct arch_dis_ctx arch;
};

#undef P
#define P(x) jump_dis_##x

#define tdis_ctx struct jump_dis_ctx *

static void jump_dis_add_to_queue(struct jump_dis_ctx *ctx, uint_tptr pc) {
    size_t diff = (pc - ctx->pc_patch_start) / MIN_INSN_SIZE;
    if (diff >= JUMP_ANALYSIS_MAX_INSNS) {
#ifdef JUMP_DIS_VERBOSE
        printf("jump-dis: not adding %llx - out of range\n",
               (unsigned long long) pc);
#endif
        return;
    }
    if (ctx->seen_mask[diff / 8] & 1 << (diff % 8)) {
#ifdef JUMP_DIS_VERBOSE
        printf("jump-dis: not adding %llx - already seen\n",
               (unsigned long long) pc);
#endif
        return;
    }
    ctx->seen_mask[diff / 8] |= 1 << (diff % 8);

    vec_append_uint_tptr(&ctx->queue.v, pc);
}

static INLINE UNUSED
void jump_dis_data(UNUSED struct jump_dis_ctx *ctx,
                   UNUSED unsigned o0, UNUSED unsigned o1, UNUSED unsigned o2,
                   UNUSED unsigned o3, UNUSED unsigned out_mask) {
    /* on ARM, ignore mov PC jumps, as they're unlikely to be in the same
     * function */
}

static INLINE UNUSED
void jump_dis_pcrel(struct jump_dis_ctx *ctx, uint_tptr dpc,
                    UNUSED struct arch_pcrel_info info) {
    ctx->bad_insn = dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end;
}

static INLINE UNUSED
void jump_dis_indirect_call(UNUSED struct jump_dis_ctx *ctx) {
}

static INLINE UNUSED
void jump_dis_ret(struct jump_dis_ctx *ctx) {
    if (ctx->pc_ret > ctx->base.pc)
        ctx->pc_ret = ctx->base.pc;
    ctx->continue_after_this_insn = false;
}

static NOINLINE UNUSED
void jump_dis_branch(struct jump_dis_ctx *ctx, uint_tptr dpc, int cc) {
    if (dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end) {
        ctx->bad_insn = true;
        return;
    }
#ifdef JUMP_DIS_VERBOSE
    printf("jump-dis: enqueueing %llx\n", (unsigned long long) dpc);
#endif
    jump_dis_add_to_queue(ctx, dpc);
    ctx->continue_after_this_insn = cc & (CC_CONDITIONAL | CC_CALL);
}

static INLINE UNUSED
void jump_dis_unidentified(UNUSED struct jump_dis_ctx *ctx) {
}

static INLINE UNUSED
void jump_dis_bad(struct jump_dis_ctx *ctx) {
    ctx->continue_after_this_insn = false;
}

static INLINE UNUSED
void jump_dis_thumb_it(UNUSED struct jump_dis_ctx *ctx) {
}

static void jump_dis_dis(struct jump_dis_ctx *ctx);

bool jump_dis_main(void *code_ptr, uint_tptr pc_patch_start,
                   uint_tptr pc_patch_end,
                   struct arch_dis_ctx initial_dis_ctx) {
    bool ret;
    struct jump_dis_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.pc_patch_start = pc_patch_start;
    ctx.pc_patch_end = pc_patch_end;
    ctx.pc_ret = -1;
    ctx.base.pc = pc_patch_end;
    ctx.arch = initial_dis_ctx;
    VEC_STORAGE_INIT(&ctx.queue, uint_tptr);
    while (1) {
        ctx.bad_insn = false;
        ctx.continue_after_this_insn = true;
        ctx.base.ptr = code_ptr + (ctx.base.pc - pc_patch_start);
        jump_dis_dis(&ctx);
#ifdef JUMP_DIS_VERBOSE
        printf("jump-dis: pc=%llx op=%08x size=%x bad=%d continue_after=%d\n",
            (unsigned long long) ctx.base.pc,
#if defined(TARGET_x86_64) || defined(TARGET_i386)
            0,
#else
            ctx.base.op,
#endif
            ctx.base.op_size,
            ctx.bad_insn,
            ctx.continue_after_this_insn);
#endif
        if (ctx.bad_insn) {
            ret = true;
            goto fail;
        }
        if (ctx.continue_after_this_insn)
            jump_dis_add_to_queue(&ctx, ctx.base.pc + ctx.base.op_size);

        /* get next address */
        do {
            if (!ctx.queue.v.length)
                goto done;
            ctx.base.pc = vec_pop_uint_tptr(&ctx.queue.v);
        } while (ctx.base.pc > ctx.pc_ret);
    }
done:
    /* no bad instructions! */
    ret = false;
fail:
    vec_free_storage_uint_tptr(&ctx.queue.v);
    return ret;
}

#include stringify(TARGET_DIR/dis-main.inc.h)
#endif /* TARGET_DIS_SUPPORTED */
