#include "substitute.h"
#include "substitute-internal.h"

EXPORT
const char *substitute_strerror(int err) {
    #define CASE(code) case code: (void) __COUNTER__; return #code
    switch (err) {
        /* substitute.h */
        enum { _start = __COUNTER__ };
        CASE(SUBSTITUTE_OK);
        CASE(SUBSTITUTE_ERR_FUNC_TOO_SHORT);
        CASE(SUBSTITUTE_ERR_FUNC_BAD_INSN_AT_START);
        CASE(SUBSTITUTE_ERR_FUNC_CALLS_AT_START);
        CASE(SUBSTITUTE_ERR_FUNC_JUMPS_TO_START);
        CASE(SUBSTITUTE_ERR_OOM);
        CASE(SUBSTITUTE_ERR_VM);
        CASE(SUBSTITUTE_ERR_NOT_ON_MAIN_THREAD);
        CASE(SUBSTITUTE_ERR_UNEXPECTED_PC_ON_OTHER_THREAD);
        CASE(SUBSTITUTE_ERR_OUT_OF_RANGE);
        CASE(SUBSTITUTE_ERR_UNKNOWN_RELOCATION_TYPE);
        CASE(SUBSTITUTE_ERR_NO_SUCH_SELECTOR);
        CASE(SUBSTITUTE_ERR_ADJUSTING_THREADS);
        _Static_assert(__COUNTER__ - _start ==
                       _SUBSTITUTE_CURRENT_MAX_ERR_PLUS_ONE + 1,
                       "not all errors named in strerror.c");
        /* substitute-internal.h */
        CASE(SUBSTITUTE_ERR_TASK_FOR_PID);
        CASE(SUBSTITUTE_ERR_MISC);
        default:
            return "(unknown libsubstitute error)";
    }
    #undef CASE
}
