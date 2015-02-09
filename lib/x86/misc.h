#pragma once
#ifdef TARGET_x86_64
#define TARGET_POINTER_SIZE 8
#else
#define TARGET_POINTER_SIZE 4
#endif
#define TARGET_DIS_SUPPORTED
