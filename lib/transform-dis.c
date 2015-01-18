#include "substitute-internal.h"
#ifndef TARGET_UNSUPPORTED
    #include "transform-dis.inc.h"
    #ifdef TARGET_arm
        #include "transform-dis-arm-multi.inc.h"
    #else
        #error ?
    #endif
#endif
