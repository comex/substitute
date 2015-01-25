#pragma once
#define TARGET_DIS_SUPPORTED
#define TARGET_DIS_HEADER "arm/dis-arm-multi.inc.h"
#define TARGET_JUMP_PATCH_HDR "arm/jump-patch.h"
#define TARGET_TRANSFORM_DIS_HEADER "arm/transform-dis-arm-multi.inc.h"
#define MIN_INSN_SIZE 2
struct arch_dis_ctx {
    unsigned thumb_it_length;
    bool pc_low_bit;
};
enum { IS_LDRD_STRD = 1 << 16 };

#define JUMP_PATCH_SIZE 8
#define MAX_REWRITTEN_SIZE (12 * 4) /* actually should be less */
