#ifdef __APPLE__

#include <stdint.h>
#include <stdbool.h>

#include "substitute.h"
#include "substitute-internal.h"
#include "darwin/read.h"

struct interpose_state {
    size_t nsegments;
    segment_command_x **segments;
    size_t max_segments;
    uintptr_t slide;
    const struct substitute_import_hook *hooks;
    size_t nhooks;
    segment_command_x *stack_segments[32];
};

static int try_bind_section(void *bind, size_t size,
                            const struct interpose_state *st, bool lazy) {
    void *ptr = bind, *end = bind + size;
    char *sym = NULL;
    uint8_t type = lazy ? BIND_TYPE_POINTER : 0;
    uint64_t addend = 0;
    uint64_t offset = 0, added_offset;
    void *segment = NULL;
    while (ptr < end) {
        uint8_t byte = *(uint8_t *) ptr;
        ptr++;
        uint8_t immediate = byte & BIND_IMMEDIATE_MASK;
        uint8_t opcode = byte & BIND_OPCODE_MASK;

        uint64_t count, stride;

        switch(opcode) {
        case BIND_OPCODE_DONE:
        case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
        case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
            break;
        case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
            read_leb128(&ptr, end, false, NULL);
            break;
        case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
            read_cstring(&ptr, end, &sym);
            /* ignoring flags for now */
            break;
        case BIND_OPCODE_SET_TYPE_IMM:
            type = immediate;
            break;
        case BIND_OPCODE_SET_ADDEND_SLEB:
            read_leb128(&ptr, end, true, &addend);
            break;
        case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            if (immediate < st->nsegments)
                segment = (void *) (st->segments[immediate]->vmaddr + st->slide);
            read_leb128(&ptr, end, false, &offset);
            break;
        case BIND_OPCODE_ADD_ADDR_ULEB:
            read_leb128(&ptr, end, false, &added_offset);
            offset += added_offset;
            break;
        case BIND_OPCODE_DO_BIND:
            count = 1;
            stride = sizeof(void *);
            goto bind;
        case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
            count = 1;
            read_leb128(&ptr, end, false, &stride);
            stride += sizeof(void *);
            goto bind;
        case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
            count = 1;
            stride = immediate * sizeof(void *) + sizeof(void *);
            goto bind;
        case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
            read_leb128(&ptr, end, false, &count);
            read_leb128(&ptr, end, false, &stride);
            stride += sizeof(void *);
            goto bind;
        bind:
            if (segment && sym) {
                const struct substitute_import_hook *h;
                size_t i;
                for (i = 0; i < st->nhooks; i++) {
                    h = &st->hooks[i];
                    if (!strcmp(sym, h->name))
                        break;
                }
                if (i != st->nhooks) {
                    while (count--) {
                        uintptr_t new = (uintptr_t) h->replacement +
                                        (intptr_t) addend;
                        uintptr_t old;
                        void *p = (void *) (segment + offset);
                        switch (type) {
                        case BIND_TYPE_POINTER: {
                            old = __atomic_exchange_n((uintptr_t *) p,
                                                      new, __ATOMIC_RELAXED);
                            break;
                        }
                        case BIND_TYPE_TEXT_ABSOLUTE32: {
                            if ((uint32_t) new != new) {
                                /* text rels should only show up on i386, where
                                 * this is impossible... */
                                substitute_panic("bad TEXT_ABSOLUTE32 rel\n");
                            }
                            old = __atomic_exchange_n((uint32_t *) p,
                                                      (uint32_t) new,
                                                      __ATOMIC_RELAXED);
                            break;
                        }
                        case BIND_TYPE_TEXT_PCREL32: {
                            uintptr_t pc = (uintptr_t) p + 4;
                            uintptr_t rel = new - pc;
                            if ((uint32_t) rel != rel) {
                                /* ditto */
                                substitute_panic("bad TEXT_ABSOLUTE32 rel\n");
                            }
                            old = __atomic_exchange_n((uint32_t *) p,
                                                      (uint32_t) rel,
                                                      __ATOMIC_RELAXED);
                            old += pc;
                            break;
                        }
                        default:
                            substitute_panic("unknown relocation type\n");
                            break;
                        }
                        if (h->old_ptr)
                            *(uintptr_t *) h->old_ptr = old - addend;
                        offset += stride;
                    }
                    break;
                }
            }
            offset += count * stride;
            break;
        }
    }
    return SUBSTITUTE_OK;
}

static void *off_to_addr(const struct interpose_state *st, uint32_t off) {
    for (size_t i = 0; i < st->nsegments; i++) {
        const segment_command_x *sc = st->segments[i];
        if ((off - sc->fileoff) < sc->filesize)
            return (void *) (sc->vmaddr + st->slide + off - sc->fileoff);
    }
    return NULL;
}

EXPORT
int substitute_interpose_imports(const struct substitute_image *image,
                                 const struct substitute_import_hook *hooks,
                                 size_t nhooks,
                                 struct substitute_import_hook_record **recordp,
                                 int options) {
    int ret = SUBSTITUTE_OK;

    if (options != 0)
        substitute_panic("%s: unrecognized options\n", __func__);

    if (recordp)
        *recordp = NULL;

    struct interpose_state st;
    st.slide = image->slide;
    st.nsegments = 0;
    st.hooks = hooks;
    st.nhooks = nhooks;
    st.segments = st.stack_segments;
    st.max_segments = sizeof(st.stack_segments) / sizeof(*st.stack_segments);

    const mach_header_x *mh = image->image_header;
    const struct load_command *lc = (void *) (mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == LC_SEGMENT_X) {
            segment_command_x *sc = (void *) lc;
            if (st.nsegments == st.max_segments) {
                segment_command_x **new = calloc(st.nsegments * 2,
                                                 sizeof(*st.segments));
                if (!new)
                    substitute_panic("%s: out of memory\n", __func__);
                memcpy(new, st.segments, st.nsegments * sizeof(*st.segments));
                if (st.segments != st.stack_segments)
                    free(st.segments);
                st.segments = new;
            }
            st.segments[st.nsegments++] = sc;
        }
        lc = (void *) lc + lc->cmdsize;
    }

    lc = (void *) (mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == LC_DYLD_INFO || lc->cmd == LC_DYLD_INFO_ONLY) {
            struct dyld_info_command *dc = (void *) lc;
            int ret;
            if ((ret = try_bind_section(off_to_addr(&st, dc->bind_off),
                                        dc->bind_size, &st, false)) ||
                (ret = try_bind_section(off_to_addr(&st, dc->weak_bind_off),
                                        dc->weak_bind_size, &st, false)) ||
                (ret = try_bind_section(off_to_addr(&st, dc->lazy_bind_off),
                                        dc->lazy_bind_size, &st, true)))
                goto fail;

            break;
        }
        lc = (void *) lc + lc->cmdsize;
    }
fail:
    if (st.segments != st.stack_segments)
        free(st.segments);
    return ret;
}

#endif /* __APPLE__ */
