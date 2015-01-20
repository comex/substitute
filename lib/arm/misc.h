#pragma once
#define TARGET_DIS_SUPPORTED
#define TARGET_DIS_HEADER "arm/dis-arm-multi.inc.h"
#define TARGET_TRANSFORM_DIS_HEADER "arm/transform-dis-arm-multi.inc.h"
#define MIN_INSN_SIZE 2
struct arch_dis_ctx { unsigned thumb_it_length; };
enum { IS_LDRD_STRD = 1 << 16 };
