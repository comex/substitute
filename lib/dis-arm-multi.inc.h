#include "dis-thumb.inc.h"
#include "dis-thumb2.inc.h"
#include "dis-arm.inc.h"

static INLINE void P(dis)(tdis_ctx ctx) {
    if (ctx->pc_low_bit) {
        uint16_t op = *(uint16_t *) ctx->ptr;
        bool is_32 = (op >> 13 & 7) == 7 && (op >> 11 & 3) != 0;
        if (is_32)
            return P(dis_thumb2)(ctx);
        else
            return P(dis_thumb)(ctx);
    } else {
        return P(dis_arm)(ctx);
    }
}
