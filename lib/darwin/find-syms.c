#ifdef __APPLE__

#include <stdbool.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <limits.h>
#include <fcntl.h>

#include "substitute.h"
#include "substitute-internal.h"
#include "dyld_cache_format.h"

extern const struct dyld_all_image_infos *_dyld_get_all_image_infos();

static pthread_once_t dyld_inspect_once = PTHREAD_ONCE_INIT;
/* and its fruits: */
static uintptr_t (*ImageLoaderMachO_getSlide)(void *);
static const struct mach_header *(*ImageLoaderMachO_machHeader)(void *);

static const struct dyld_cache_header *_Atomic s_cur_shared_cache_hdr;
static int s_cur_shared_cache_fd;
static pthread_once_t s_open_cache_once = PTHREAD_ONCE_INIT;
static struct dyld_cache_local_symbols_info s_cache_local_symbols_info;
static struct dyld_cache_local_symbols_entry *s_cache_local_symbols_entries;

static bool oscf_try_dir(const char *dir, const char *arch,
                         const struct dyld_cache_header *dch) {
    char path[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/%s%s", dir,
                 DYLD_SHARED_CACHE_BASE_NAME, arch) >= sizeof(path))
        return false;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    struct dyld_cache_header this_dch;
    if (read(fd, &this_dch, sizeof(this_dch)) != sizeof(this_dch))
        goto fail;
    if (memcmp(this_dch.uuid, dch->uuid, 16) ||
        this_dch.localSymbolsSize != dch->localSymbolsSize /* just in case */)
        goto fail;
    struct dyld_cache_local_symbols_info *lsi = &s_cache_local_symbols_info;
    if (pread(fd, lsi, sizeof(*lsi), dch->localSymbolsOffset) != sizeof(*lsi))
        goto fail;
    if (lsi->nlistOffset > dch->localSymbolsSize ||
        lsi->nlistCount > (dch->localSymbolsSize - lsi->nlistOffset)
                           / sizeof(substitute_sym) ||
        lsi->stringsOffset > dch->localSymbolsSize ||
        lsi->stringsSize > dch->localSymbolsSize - lsi->stringsOffset) {
        /* bad format */
        goto fail;
    }
    uint32_t count = lsi->entriesCount;
    if (count > 1000000)
        goto fail;
    struct dyld_cache_local_symbols_entry *lses;
    size_t lses_size = count * sizeof(*lses);
    if (!(lses = malloc(lses_size)))
        goto fail;
    if (pread(fd, lses, lses_size, dch->localSymbolsOffset + lsi->entriesOffset)
        != lses_size) {
        free(lses);
        goto fail;
    }

    s_cur_shared_cache_fd = fd;
    s_cache_local_symbols_entries = lses;
    return true;

fail:
    memset(lsi, 0, sizeof(*lsi));
    close(fd);
    return false;
}

static void open_shared_cache_file_once() {
    s_cur_shared_cache_fd = -1;
    const struct dyld_cache_header *dch = s_cur_shared_cache_hdr;
    if (memcmp(dch->magic, "dyld_v1 ", 8))
        return;
    if (dch->localSymbolsSize < sizeof(struct dyld_cache_local_symbols_info))
        return;
    const char *archp = &dch->magic[8];
    while (*archp == ' ')
        archp++;
    static char filename[32];
    const char *env_dir = getenv("DYLD_SHARED_CACHE_DIR");
    if (env_dir) {
        if (oscf_try_dir(env_dir, archp, dch))
            return;
    }
#if __IPHONE_OS_VERSION_MIN_REQUIRED
    oscf_try_dir(IPHONE_DYLD_SHARED_CACHE_DIR, archp, dch);
#else
    oscf_try_dir(MACOSX_DYLD_SHARED_CACHE_DIR, archp, dch);
#endif
}

static bool ul_mmap(int fd, off_t offset, size_t size,
                    void *data_p, void **mapping_p, size_t *mapping_size_p) {
    int pmask = getpagesize() - 1;
    int page_off = offset & pmask;
    off_t map_offset = offset & ~pmask;
    size_t map_size = ((offset + size + pmask) & ~pmask) - map_offset;
    void *mapping = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, map_offset);
    if (mapping == MAP_FAILED)
        return false;
    *(void **) data_p = (char *) mapping + page_off;
    *mapping_p = mapping;
    *mapping_size_p = map_size;
    return true;
}

static bool get_shared_cache_syms(const void *hdr,
                                  const substitute_sym **syms_p,
                                  const char **strs_p,
                                  size_t *nsyms_p,
                                  void **mapping_p,
                                  size_t *mapping_size_p) {
    pthread_once(&s_open_cache_once, open_shared_cache_file_once);
    int fd = s_cur_shared_cache_fd;
    if (fd == -1)
        return false;
    const struct dyld_cache_header *dch = s_cur_shared_cache_hdr;
    const struct dyld_cache_local_symbols_info *lsi = &s_cache_local_symbols_info;
    struct dyld_cache_local_symbols_entry lse;
    for (uint32_t i = 0; i < lsi->entriesCount; i++) {
        lse = s_cache_local_symbols_entries[i];
        if (lse.dylibOffset == (uintptr_t) hdr - (uintptr_t) dch)
            goto got_lse;
    }
    return false;
got_lse:
    /* map - we don't do this persistently to avoid wasting address space on
     * iOS (my random OS X 10.10 blob pushes 55MB) */
    if (lse.nlistStartIndex > lsi->nlistCount ||
        lsi->nlistCount - lse.nlistStartIndex < lse.nlistCount)
        return false;

    char *ls_data;
    if (!ul_mmap(fd, dch->localSymbolsOffset, dch->localSymbolsSize,
                 &ls_data, mapping_p, mapping_size_p))
        return false;
    const substitute_sym *syms = (void *) (ls_data + lsi->nlistOffset);
    *syms_p = syms + lse.nlistStartIndex;
    *strs_p = ls_data + lsi->stringsOffset;
    *nsyms_p = lse.nlistCount;
    return true;
}


static const struct dyld_cache_header *get_cur_shared_cache_hdr() {
    const struct dyld_cache_header *dch = s_cur_shared_cache_hdr;
    if (!dch) {
        /* race is OK */
        uint64_t start_address = 0;
        if (syscall(294, &start_address)) /* shared_region_check_np */
            dch = (void *) 1;
        else
            dch = (void *) (uintptr_t) start_address;
        s_cur_shared_cache_hdr = dch;
    }
    return dch == (void *) 1 ? NULL : dch;
}

static bool addr_in_shared_cache(const void *addr) {
    const struct dyld_cache_header *dch = get_cur_shared_cache_hdr();
    if (!dch)
        return false;

    uint32_t mapping_count = dch->mappingCount;
    const struct dyld_cache_mapping_info *mappings =
        (void *) ((char *) dch + dch->mappingOffset);
    intptr_t slide = (uintptr_t) dch - (uintptr_t) mappings[0].address;

    for (uint32_t i = 0; i < mapping_count; i++) {
        const struct dyld_cache_mapping_info *mapping = &mappings[i];
        uintptr_t diff = (uintptr_t) addr -
                         ((uintptr_t) mapping->address + slide);
        if (diff < mapping->size)
            return true;
    }
    return false;
}

static void *sym_to_ptr(const substitute_sym *sym, intptr_t slide) {
    uintptr_t addr = sym->n_value;
    addr += slide;
    if (sym->n_desc & N_ARM_THUMB_DEF)
        addr |= 1;
    return (void *) addr;
}

static void find_syms_raw(const void *hdr, intptr_t *restrict slide,
                          const char **restrict names, void **restrict syms,
                          size_t nsyms) {
    memset(syms, 0, sizeof(*syms) * nsyms);

    void *mapping = NULL;
    size_t mapping_size = 0;
    const substitute_sym *cache_syms = NULL;
    const char *cache_strs = NULL;
    size_t ncache_syms = 0;
    if (addr_in_shared_cache(hdr))
        get_shared_cache_syms(hdr, &cache_syms, &cache_strs, &ncache_syms,
                              &mapping, &mapping_size);

    /* note: no verification at all */
    const mach_header_x *mh = hdr;
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
    return; /* no symtab, no symbols */
ok: ;
    substitute_sym *symtab = NULL;
    const char *strtab = NULL;
    lc = (void *) (mh + 1);
    for (uint32_t i = 0; i < ncmds; i++) {
        if (lc->cmd == LC_SEGMENT_X) {
            segment_command_x *sc = (void *) lc;
            if (syc.symoff - sc->fileoff < sc->filesize)
                symtab = (void *) sc->vmaddr + syc.symoff - sc->fileoff;
            if (syc.stroff - sc->fileoff < sc->filesize)
                strtab = (void *) sc->vmaddr + syc.stroff - sc->fileoff;
            if (*slide == -1 && sc->fileoff == 0) {
                // used only for dyld
                *slide = (uintptr_t) hdr - sc->vmaddr;
            }
            if (symtab && strtab)
                goto ok2;
        }
        lc = (void *) lc + lc->cmdsize;
    }
    return; /* uh... weird */
ok2: ;
    symtab = (void *) symtab + *slide;
    strtab = (void *) strtab + *slide;
    size_t found_syms = 0;

    for (int type = 0; type <= 1; type++) {
        const substitute_sym *this_symtab = type ? cache_syms : symtab;
        const char *this_strtab = type ? cache_strs : strtab;
        size_t this_nsyms = type ? ncache_syms : syc.nsyms;
        /* This could be optimized for efficiency with a large number of
         * names... */
        for (uint32_t i = 0; i < this_nsyms; i++) {
            const substitute_sym *sym = &this_symtab[i];
            uint32_t strx = sym->n_un.n_strx;
            const char *name = strx == 0 ? "" : this_strtab + strx;
            for (size_t j = 0; j < nsyms; j++) {
                if (!syms[j] && !strcmp(name, names[j])) {
                    syms[j] = sym_to_ptr(sym, *slide);
                    if (++found_syms == nsyms)
                        goto end;
                }
            }
        }
    }

end:
    if (mapping_size)
        munmap(mapping, mapping_size);
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

    const char *names[2] = { "__ZNK16ImageLoaderMachO8getSlideEv",
                             "__ZNK16ImageLoaderMachO10machHeaderEv" };
    void *syms[2];
    intptr_t dyld_slide = -1;
    find_syms_raw(dyld_hdr, &dyld_slide, names, syms, 2);
    if (!syms[0] || !syms[1])
        substitute_panic("couldn't find ImageLoader methods\n");
    ImageLoaderMachO_getSlide = syms[0];
    ImageLoaderMachO_machHeader = syms[1];
}

/* 'dlhandle' keeps the image alive */
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
int substitute_find_private_syms(struct substitute_image *im,
                                 const char **restrict names,
                                 void **restrict syms,
                                 size_t nsyms) {
    find_syms_raw(im->image_header, &im->slide, names, syms, nsyms);
    return SUBSTITUTE_OK;
}

#endif /* __APPLE__ */
