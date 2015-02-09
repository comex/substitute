/* Pretty trivial, but in its own file to match the other architectures. */
#include "x86/jump-patch.h"

static void transform_dis_pcrel(struct transform_dis_ctx *ctx, uint64_t dpc,
                                struct arch_pcrel_info info) {
    /* push %reg; mov $dpc, %reg; <orig but with reg instead>; pop %reg */
    /* reg is rcx, or rax if the instruction might be using rcx. */
    int rax = info.reg == 1;
    void *code = *ctx->rewritten_ptr_ptr;
    /* push */
    op8(&code, rax ? 0x50 : 0x51);
    /* mov */
#ifdef TARGET_x86_64
    op8(&code, 0x48);
    op8(&code, rax ? 0xb8 : 0xb9);
    op64(&code, dpc);
#else
    op8(&code, rax ? 0xb8 : 0xb9);
    op32(&code, dpc);
#endif
    ctx->write_newop_here = code;
    code += ctx->base.op_size;
    /* pop */
    op8(&code, rax ? 0x58 : 0x59);
    *ctx->rewritten_ptr_ptr = code;
    ctx->base.newop[0] = rax ? 0 : 1;
    ctx->base.modify = true;
}

static void transform_dis_branch(struct transform_dis_ctx *ctx, uint_tptr dpc,
                                 int cc) {
    if (dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end) {
        ctx->err = SUBSTITUTE_ERR_FUNC_BAD_INSN_AT_START;
        return;
    }
    void *code = *ctx->rewritten_ptr_ptr;

    ctx->write_newop_here = code;
    code += ctx->base.op_size;

    struct arch_dis_ctx arch;
    uintptr_t source = (uintptr_t) code + 2;
    int size = jump_patch_size(source, dpc, arch, true);
    /* if not taken, jmp past the big jump - this is a bit suboptimal but not that bad */
    op8(&code, 0xeb);
    op8(&code, size);
    make_jump_patch(&code, source, dpc, arch);

    *ctx->rewritten_ptr_ptr = code;
    ctx->base.newop[0] = 2;
    ctx->base.modify = true;

    if (!cc)
        transform_dis_ret(ctx);
}

static void transform_dis_pre_dis(UNUSED struct transform_dis_ctx *ctx) {}
static void transform_dis_post_dis(UNUSED struct transform_dis_ctx *ctx) {}
