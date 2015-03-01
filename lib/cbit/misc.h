#pragma once

#define UNUSED_STATIC_INLINE __attribute__((unused)) static inline

#define LET_LOOP__(expr, ctr) \
    if (0) \
       __done_##ctr: continue; \
    else if (0) \
       __break_##ctr: break; \
    else \
        for (expr; ;) \
            if (1) \
                goto __body_##ctr; \
            else \
                for (;;) \
                    if (1) \
                        goto __break_##ctr; \
                    else \
                        for (;;) \
                            if (1) \
                                goto __done_##ctr; \
                            else \
                                __body_##ctr:

#define LET_LOOP_(expr, ctr) LET_LOOP__(expr, ctr)
#define LET_LOOP(expr) LET_LOOP_(expr, __COUNTER__)

#define LET__(expr, ctr) \
    if (0) \
      __done_##ctr:; \
    else \
        for (expr; ;) \
            if (1) \
                goto __body_##ctr; \
            else \
                for (;;) \
                    if (1) \
                        goto __done_##ctr; \
                    else \
                            __body_##ctr:

#define LET_(expr, ctr) LET__(expr, ctr)
#define LET(expr) LET_(expr, __COUNTER__)

/* XXX */
#define safe_mul(a, b) ((a) * (b))
#define safe_add(a, b) ((a) + (b))

