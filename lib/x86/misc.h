#pragma once
#define TARGET_DIS_SUPPORTED
#define TARGET_DIS_HEADER "x86/dis-x86.inc.h"
#define TARGET_JUMP_PATCH_HDR "x86/jump-patch.h"
#define MIN_INSN_SIZE 1
#define TD_MAX_REWRITTEN_SIZE 100 /* XXX */

struct arch_dis_ctx {};
static inline void arch_dis_ctx_init(UNUSED struct arch_dis_ctx *ctx) {}
