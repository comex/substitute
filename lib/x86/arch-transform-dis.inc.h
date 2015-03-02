/* Pretty trivial, but in its own file to match the other architectures. */
#include "x86/jump-patch.h"

static inline void push_mov_head(void **code, uint64_t imm, bool rax) {
    /* push */
    op8(code, rax ? 0x50 : 0x51);
    /* mov */
#ifdef TARGET_x86_64
    op8(code, 0x48);
    op8(code, rax ? 0xb8 : 0xb9);
    op64(code, imm);
#else
    op8(code, rax ? 0xb8 : 0xb9);
    op32(code, imm);
#endif
}

static inline void push_mov_tail(void **code, bool rax) {
    /* pop */
    op8(code, rax ? 0x58 : 0x59);
}

UNUSED
static void transform_dis_pcrel(struct transform_dis_ctx *ctx, uint64_t dpc,
                                struct arch_pcrel_info info) {
    /* push %reg; mov $dpc, %reg; <orig but with reg instead>; pop %reg
     * -or, if jump-
     * push %reg; mov $dpc, %reg; mov %reg, -8(%rsp); pop %reg;
     *      <orig but with -0x10(%rsp)>
     * reg is rcx, or rax if the instruction might be using rcx.
     * Max size: 11 + orig + 1
     * Minimum size is 6 bytes, so there could be at most 1 in a patch area. */
    void *code = *ctx->rewritten_ptr_ptr;
    if (info.is_jump) {
        push_mov_head(&code, dpc, true);
        memcpy(code, ((uint8_t[]) {0x48, 0x89, 0x44, 0x24, 0xf8}), 5);
        code += 5;
        push_mov_tail(&code, true);
        ctx->write_newop_here = code;
        code += ctx->base.op_size - 2;
        ctx->base.newval[0] = 4; /* esp */
        ctx->base.newval[1] = -0x10;
    } else {
        bool rax = info.reg == 1;
        push_mov_head(&code, dpc, rax);
        ctx->write_newop_here = code;
        code += ctx->base.op_size - 4 /* see dis-main.inc.h */;
        push_mov_tail(&code, rax);
        ctx->base.newval[0] = rax ? 0 /* rcx */ : 1 /* rax */;
        ctx->base.newval[1] = 0;
    }
    *ctx->rewritten_ptr_ptr = code;
    ctx->base.modify = true;
}

static void transform_dis_branch(struct transform_dis_ctx *ctx, uint_tptr dpc,
                                 int cc) {
    if (dpc == ctx->base.pc + ctx->base.op_size && (cc & CC_CALL)) {
        /* Probably a poor man's PC-rel - 'call .; pop %some'.
         * Push the original address.
         * Max size: orig + 1 + 11 + 5 + 1
         * Minimum call size is 4 bytes; at most 2. */
        void *code = *ctx->rewritten_ptr_ptr;
        ctx->write_newop_here = NULL;

        /* push %whatever */
        op8(&code, 0x50);
        /* push %rax; mov $dpc, %rax */
        push_mov_head(&code, dpc, true);
        /* mov %rax, 8(%rsp) / mov %eax, 4(%esp) */
#ifdef TARGET_x86_64
        memcpy(code, ((uint8_t[]) {0x48, 0x8b, 0x44, 0x24, 0x08}), 5);
        code += 5;
#else
        memcpy(code, ((uint8_t[]) {0x89, 0x44, 0x24, 0x04}), 4);
        code += 4;
#endif
        /* pop %rax */
        push_mov_tail(&code, true);

        *ctx->rewritten_ptr_ptr = code;
        return;
    }
    transform_dis_branch_top(ctx, dpc, cc);
    void *code = *ctx->rewritten_ptr_ptr;
    struct arch_dis_ctx arch;

    if (cc & CC_CONDITIONAL) {
        ctx->write_newop_here = code;
        code += ctx->base.op_size;

        uint_tptr source = ctx->pc_trampoline + ctx->base.op_size + 2;
        int size = jump_patch_size(source, dpc, arch, true);

        /* If not taken, jmp past the big jump - this is a bit suboptimal but not
         * that bad.
         * Max size: orig + 2 + 14
         * Minimum jump size is 2 bytes; at most 3. */
        op8(&code, 0xeb);
        op8(&code, size);

        make_jump_patch(&code, source, dpc, arch);

        ctx->base.newval[0] = 2;
        ctx->base.modify = true;
        transform_dis_ret(ctx);
    } else {
        ctx->write_newop_here = NULL;

        make_jmp_or_call(&code, ctx->pc_trampoline, dpc, cc & CC_CALL);
    }
    *ctx->rewritten_ptr_ptr = code;
}

static void transform_dis_pre_dis(UNUSED struct transform_dis_ctx *ctx) {}
static void transform_dis_post_dis(UNUSED struct transform_dis_ctx *ctx) {}
