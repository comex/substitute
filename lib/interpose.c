#ifdef __APPLE__


#include <stdint.h>
#include <stdbool.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>


#include "substitute.h"
#include "substitute-internal.h"


enum { MAX_SEGMENTS = 32 };
struct interpose_state {
    int nsegments;
    segment_command_x *segments[MAX_SEGMENTS];
    uintptr_t slide;
	const struct substitute_import_hook *hooks;
	size_t nhooks;
};

static uintptr_t read_leb128(void **ptr, void *end, bool is_signed) {
    uintptr_t result = 0;
    uint8_t *p = *ptr;
    uint8_t bit;
    unsigned int shift = 0;
    do {
        if (p >= (uint8_t *) end)
            return 0;
        bit = *p++;
        uintptr_t k = bit & 0x7f;
        if (shift < sizeof(uintptr_t) * 8)
            result |= k << shift;
        shift += 7;
    } while (bit & 0x80);
    if (is_signed && (bit & 0x40))
        result |= ~((uintptr_t) 0) << shift;
    *ptr = p;
    return result;
}

static inline char *read_cstring(void **ptr, void *end) {
    char *s = *ptr;
    *ptr = s + strnlen(s, (char *) end - s);
    return s;
}


static int try_bind_section(void *bind, size_t size, const struct interpose_state *st, bool lazy) {
    void *ptr = bind, *end = bind + size;
    const char *sym = NULL;
    uint8_t type = lazy ? BIND_TYPE_POINTER : 0;
    intptr_t addend = 0;
    size_t offset = 0;
    int n = 0;
    void *segment = NULL;
    while (ptr < end) {
        uint8_t byte = *(uint8_t *) ptr;
        ptr++;
        uint8_t immediate = byte & BIND_IMMEDIATE_MASK;
        uint8_t opcode = byte & BIND_OPCODE_MASK;

        uintptr_t count, stride;

        switch(opcode) {
        case BIND_OPCODE_DONE:
        case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
        case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
            break;
        case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
            read_leb128(&ptr, end, false);
            break;
        case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
            sym = read_cstring(&ptr, end);
            /* ignoring flags for now */
            break;
        case BIND_OPCODE_SET_TYPE_IMM:
            type = immediate;
            break;
        case BIND_OPCODE_SET_ADDEND_SLEB:
            addend = read_leb128(&ptr, end, true);
            break;
        case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            if (immediate < st->nsegments)
                segment = (void *) (st->segments[immediate]->vmaddr + st->slide);
            offset = read_leb128(&ptr, end, false);
            break;
        case BIND_OPCODE_ADD_ADDR_ULEB:
            offset += read_leb128(&ptr, end, false);
            break;
        case BIND_OPCODE_DO_BIND:
            count = 1;
            stride = sizeof(void *);
            goto bind;
        case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
            count = 1;
            stride = read_leb128(&ptr, end, false) + sizeof(void *);
            goto bind;
        case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
            count = 1;
            stride = immediate * sizeof(void *) + sizeof(void *);
            goto bind;
        case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
            count = read_leb128(&ptr, end, false);
            stride = read_leb128(&ptr, end, false) + sizeof(void *);
            goto bind;
        bind:
            if (segment && sym) {
				const struct substitute_import_hook *h;
				size_t i;
				for (i = 0; i < st->nhooks; i++) {
					h = &st->hooks[i];
					// TODO abs/pcrel32? used on arm?
					if (!strcmp(sym, h->name)) {
						if (type != BIND_TYPE_POINTER)
							return SUBSTITUTE_ERR_UNKNOWN_RELOCATION_TYPE;
						break;
					}
				}
				if (i != st->nhooks) {
					while (count--) {
						uintptr_t new = (uintptr_t) h->replacement + addend;
						uintptr_t *p = (void *) (segment + offset);
						uintptr_t old = __atomic_exchange_n(p, new, __ATOMIC_RELAXED);
						if (h->old_ptr)
							*(void **) h->old_ptr = (void *) (old - addend);
						offset += stride;
					}
					break;
				}
			}
			offset += count * stride;
            break;
        }
    }
    return n;
}

static void *off_to_addr(const struct interpose_state *st, uint32_t off) {
    for (int i = 0; i < st->nsegments; i++) {
        const segment_command_x *sc = st->segments[i];
        if ((off - sc->fileoff) < sc->filesize)
            return (void *) (sc->vmaddr + st->slide + off - sc->fileoff);
    }
    return NULL;
}

EXPORT
int substitute_interpose_imports(const struct substitute_image *image,
                                 const struct substitute_import_hook *hooks,
                                 size_t nhooks, UNUSED int options) {
    struct interpose_state st;
    st.slide = image->slide;
    st.nsegments = 0;
	st.hooks = hooks;
	st.nhooks = nhooks;

	const mach_header_x *mh = image->image_header;
    const struct load_command *lc = (void *) (mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == LC_SEGMENT_X) {
            segment_command_x *sc = (void *) lc;
            if (st.nsegments < MAX_SEGMENTS)
                st.segments[st.nsegments++] = sc;
        }
        lc = (void *) lc + lc->cmdsize;
    }

    lc = (void *) (mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == LC_DYLD_INFO || lc->cmd == LC_DYLD_INFO_ONLY) {
            struct dyld_info_command *dc = (void *) lc;
			int ret;
            if ((ret = try_bind_section(off_to_addr(&st, dc->bind_off), dc->bind_size, &st, false)) ||
                (ret = try_bind_section(off_to_addr(&st, dc->weak_bind_off), dc->weak_bind_size, &st, false)) ||
                (ret = try_bind_section(off_to_addr(&st, dc->lazy_bind_off), dc->lazy_bind_size, &st, true)))
				return ret;

            break;
        }
        lc = (void *) lc + lc->cmdsize;
    }
	return SUBSTITUTE_OK;
}

#endif /* __APPLE__ */
