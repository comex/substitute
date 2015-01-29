#pragma once

#define LET_LOOP__(expr, ctr) \
   if (0) \
      __done_##ctr: continue; \
   else if (0) \
      __break_##ctr: break; \
   else \
      for (; ; ({ goto __break_##ctr; })) \
         for (expr; ; ({ goto __done_##ctr; }))

#define LET_LOOP_(expr, ctr) LET_LOOP__(expr, ctr)
#define LET_LOOP(expr) LET_LOOP_(expr, __COUNTER__)

#define LET__(expr, ctr) \
   if (0) \
      __done_##ctr: ; \
   else \
      for (expr; ; ({ goto __done_##ctr; }))

#define LET_(expr, ctr) LET__(expr, ctr)
#define LET(expr) LET_(expr, __COUNTER__)

#define safe_mul(a, b) ((a) * (b)) /* XXX */

