#pragma once
#include <stdint.h>
#include <stdbool.h>

bool jump_dis_main(void *code_ptr, uintptr_t pc_patch_start, uintptr_t pc_patch_end, bool pc_low_bit);
