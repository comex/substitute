#include "darwin/read.h"
bool read_leb128(void **ptr, void *end, bool is_signed, uint64_t *out) {
    uint64_t result = 0;
    uint8_t *p = *ptr;
    uint8_t bit;
    unsigned int shift = 0;
    do {
        if (p >= (uint8_t *) end)
            return false;
        bit = *p++;
        uint64_t k = bit & 0x7f;
        if (shift < 64)
            result |= k << shift;
        shift += 7;
    } while (bit & 0x80);
    if (is_signed && (bit & 0x40) && shift < 64)
        result |= ~((uint64_t) 0) << shift;
    *ptr = p;
    if (out)
        *out = result;
    return true;
}
