#ifdef __APPLE__

#include <stdbool.h>
#include <dlfcn.h>
#include <pthread.h>

#include "substitute.h"
#include "substitute-internal.h"
#include "find-syms.h"

extern const struct dyld_all_image_infos *_dyld_get_all_image_infos();

static pthread_once_t dyld_inspect_once = PTHREAD_ONCE_INIT;
/* and its fruits: */
static uintptr_t (*ImageLoaderMachO_getSlide)(void *);
static const struct mach_header *(*ImageLoaderMachO_machHeader)(void *);

static void *sym_to_ptr(substitute_sym *sym, intptr_t slide) {
    uintptr_t addr = sym->n_value;
    addr += slide;
    if (sym->n_desc & N_ARM_THUMB_DEF)
        addr |= 1;
    return (void *) addr;
}

static bool find_slide(const mach_header_x *mh, intptr_t *slidep) {
    uint32_t ncmds = mh->ncmds;
    struct load_command *lc = (void *) (mh + 1);
    for (uint32_t i = 0; i < ncmds; i++) {
        if (lc->cmd == LC_SEGMENT_X) {
            segment_command_x *sc = (void *) lc;
            if (sc->fileoff == 0) {
                *slidep = (uintptr_t) mh - sc->vmaddr;
                return true;
            }
        }
        lc = (void *) lc + lc->cmdsize;
    }
    return false;
}

bool find_symtab_data(const mach_header_x *mh, struct symtab_data *data) {
    uint32_t ncmds = mh->ncmds;
    struct load_command *lc = (void *) (mh + 1);
    struct symtab_command syc;
    for (uint32_t i = 0; i < ncmds; i++) {
        if (lc->cmd == LC_SYMTAB) {
            syc = *(struct symtab_command *) lc;
            goto ok;
        }
        lc = (void *) lc + lc->cmdsize;
    }
    return false; /* no symtab, no symbols */
ok: ;
    lc = (void *) (mh + 1);
    for (uint32_t i = 0; i < ncmds; i++) {
        if (lc->cmd == LC_SEGMENT_X) {
            segment_command_x *sc = (void *) lc;
            uint32_t seg_symoff = syc.symoff - sc->fileoff;
            uint32_t seg_stroff = syc.stroff - sc->fileoff;
            if (seg_symoff < sc->filesize &&
                syc.nsyms <= (sc->filesize - seg_symoff) / sizeof(substitute_sym) &&
                seg_stroff < sc->filesize &&
                syc.strsize <= sc->filesize - seg_stroff) {
                data->linkedit_vmaddr = sc->vmaddr;
                data->linkedit_symoff = seg_symoff;
                data->nsyms = syc.nsyms;
                data->linkedit_stroff = seg_stroff;
                data->strsize = syc.strsize;
                return true;
            }
        }
        lc = (void *) lc + lc->cmdsize;
    }
    return false; /* not in any segment? */
}

void find_syms_raw(const struct symtab_data *restrict data, void *linkedit,
                   const char **restrict names, substitute_sym **restrict syms,
                   size_t count) {
    memset(syms, 0, count * sizeof(*syms));
    substitute_sym *symtab = linkedit + data->linkedit_symoff;
    const char *strtab = linkedit + data->linkedit_stroff;
    /* This could be optimized for efficiency with a large number of names... */
    for (uint32_t i = 0; i < data->nsyms; i++) {
        substitute_sym *sym = &symtab[i];
        uint32_t strx = sym->n_un.n_strx;
        const char *name = (strx == 0 || strx >= data->strsize) ? "" : strtab + strx;
        size_t maxlen = data->strsize - strx;
        for (size_t j = 0; j < count; j++) {
            if (!strncmp(name, names[j], maxlen)) {
                syms[j] = sym;
                break;
            }
        }
    }
}

/* This is a mess because the usual _dyld_image_count loop is not thread safe.
 * Since it uses a std::vector and (a) erases from it (making it possible for a
 * loop to skip entries) and (b) and doesn't even lock it in
 * _dyld_get_image_header etc., this is true even if the image is guaranteed to
 * be found, including the possibility to crash.  How do we solve this?
 * Inception - we steal dyld's private symbols...  We could avoid the symbols
 * by calling the vtable of dlopen handles, but that seems unstable.  As is,
 * the method used is somewhat convoluted in an attempt to maximize stability.
 */

static void inspect_dyld() {
    const struct dyld_all_image_infos *aii = _dyld_get_all_image_infos();
    const void *dyld_hdr = aii->dyldImageLoadAddress;

    const char *names[2] = { "__ZNK16ImageLoaderMachO8getSlideEv", "__ZNK16ImageLoaderMachO10machHeaderEv" };
    substitute_sym *syms[2];
    intptr_t dyld_slide = -1;
    struct symtab_data symtab_data;
    if (!find_slide(dyld_hdr, &dyld_slide) ||
        !find_symtab_data(dyld_hdr, &symtab_data))
        substitute_panic("couldn't find ImageLoader methods\n");
    void *linkedit = (void *) (symtab_data.linkedit_vmaddr + dyld_slide);
    find_syms_raw(&symtab_data, linkedit, names, syms, 2);
    if (!syms[0] || !syms[1])
        substitute_panic("couldn't find ImageLoader methods\n");
    ImageLoaderMachO_getSlide = sym_to_ptr(syms[0], dyld_slide);
    ImageLoaderMachO_machHeader = sym_to_ptr(syms[1], dyld_slide);
}

EXPORT
struct substitute_image *substitute_open_image(const char *filename) {
    pthread_once(&dyld_inspect_once, inspect_dyld);

    void *dlhandle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    if (!dlhandle)
        return NULL;

    const void *image_header = ImageLoaderMachO_machHeader(dlhandle);
    intptr_t slide = ImageLoaderMachO_getSlide(dlhandle);

    struct substitute_image *im = malloc(sizeof(*im));
    if (!im)
        return NULL;
    im->slide = slide;
    im->dlhandle = dlhandle;
    im->image_header = image_header;
    return im;
}

EXPORT
void substitute_close_image(struct substitute_image *im) {
    dlclose(im->dlhandle); /* ignore errors */
    free(im);
}

EXPORT
int substitute_find_private_syms(struct substitute_image *im, const char **names,
                                 void **syms, size_t nsyms) {
    struct symtab_data symtab_data;
    if (!find_symtab_data(im->image_header, &symtab_data))
        return SUBSTITUTE_OK;
    void *linkedit = (void *) (symtab_data.linkedit_vmaddr + im->slide);
    substitute_sym **ssyms = (void *) syms;
    find_syms_raw(&symtab_data, linkedit, names, ssyms, nsyms);
    for (size_t i = 0; i < nsyms; i++) {
        if (ssyms[i])
            syms[i] = sym_to_ptr(ssyms[i], im->slide);
    }
    return SUBSTITUTE_OK;
}

EXPORT
void *substitute_sym_to_ptr(struct substitute_image *handle, substitute_sym *sym) {
    return sym_to_ptr(sym, handle->slide);
}

#endif /* __APPLE__ */
