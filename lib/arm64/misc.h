#pragma once
#define TARGET_DIS_SUPPORTED
#define TARGET_DIS_HEADER "arm64/dis-arm64.inc.h"
#define TARGET_JUMP_PATCH_HDR "arm64/jump-patch.h"
#define TARGET_TRANSFORM_DIS_HEADER "arm64/transform-dis-arm64.inc.h"
#define MIN_INSN_SIZE 4
#define TD_MAX_REWRITTEN_SIZE (7 * 2 * 4) /* also conservative */
struct arch_dis_ctx {};
static inline void arch_dis_ctx_init(UNUSED struct arch_dis_ctx *ctx) {}
