#pragma once
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

bool read_leb128(void **ptr, void *end, bool is_signed, uint64_t *out);

static inline bool read_cstring(void **ptr, void *end, char **out) {
    char *s = *ptr;
    size_t maxlen = (char *) end - s;
    size_t len = strnlen(s, maxlen);
    if (len == maxlen)
        return false;
    *out = s;
    *ptr = s + len + 1;
    return true;
}

