#ifdef __APPLE__

#include <stdbool.h>
#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <dlfcn.h>
#include <pthread.h>

#include "substitute.h"
#include "substitute-internal.h"

struct LibSystemHelpers {
	uintptr_t version;
	void (*acquireGlobalDyldLock)();
	void (*releaseGlobalDyldLock)();
	/* ... */
};

extern const struct dyld_all_image_infos *_dyld_get_all_image_infos();

static pthread_once_t dyld_inspect_once = PTHREAD_ONCE_INIT;
/* and its fruits: */
static struct LibSystemHelpers *libsystem_helpers;
static const struct mach_header *(*my_dyld_get_image_header)(uint32_t);
static const char *(*my_dyld_get_image_name)(uint32_t);
static intptr_t (*my_dyld_get_image_vmaddr_slide)(uint32_t);
static uint32_t (*my_dyld_image_count)();

#ifdef __LP64__
typedef struct mach_header_64 mach_header_x;
typedef struct segment_command_64 segment_command_x;
typedef struct section_64 section_x;
#define LC_SEGMENT_X LC_SEGMENT_64
#else
typedef struct mach_header mach_header_x;
typedef struct segment_command segment_command_x;
typedef struct section section_x;
#define LC_SEGMENT_X LC_SEGMENT
#endif

static void *sym_to_ptr(substitute_sym *sym, ssize_t slide) {
	uintptr_t addr = sym->n_value;
	addr += slide;
	if (sym->n_desc & N_ARM_THUMB_DEF)
		addr |= 1;
	return (void *) addr;
}

static void substitute_find_syms_raw(const void *hdr, ssize_t *slide, const char **names, substitute_sym **syms, size_t count) {
	memset(syms, 0, sizeof(*syms) * count);

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
	/* This could be optimized for efficiency with a large number of names... */
	for (uint32_t i = 0; i < syc.nsyms; i++) {
		substitute_sym *sym = &symtab[i];
		uint32_t strx = sym->n_un.n_strx;
		const char *name = strx == 0 ? "" : strtab + strx;
		for (size_t j = 0; j < count; j++) {
			if (!strcmp(name, names[j])) {
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
 * be found, including the possibility to crash.
 * How do we solve this?  Inception - we steal dyld's private symbols...
 * We could avoid the symbols by calling the vtable of dlopen handles, but that
 * seems unstable.
 */

static void inspect_dyld() {
	const struct dyld_all_image_infos *aii = _dyld_get_all_image_infos();
	const void *dyld_hdr = aii->dyldImageLoadAddress;

	const char *names[2] = { "__ZN4dyld17gLibSystemHelpersE", "__dyld_func_lookup" };
	substitute_sym *syms[2];
	ssize_t dyld_slide = -1;
	substitute_find_syms_raw(dyld_hdr, &dyld_slide, names, syms, 2);
	if (!syms[0] || !syms[1])
		panic("couldn't find dyld::gLibSystemHelpers\n");

	libsystem_helpers = *(struct LibSystemHelpers **) sym_to_ptr(syms[0], dyld_slide);
	if (!libsystem_helpers)
		panic("dyld::gLibSystemHelpers was NULL\n");

	/* We get the internal versions of _dyld_get_image_count and friends in case the normal ones are fixed in the future to use locking (in which case we'd be double locking). */
	int (*_dyld_func_lookup)(const char *name, void **address) = sym_to_ptr(syms[1], dyld_slide);
	if (!_dyld_func_lookup("__dyld_get_image_header", (void **) &my_dyld_get_image_header) ||
	    !_dyld_func_lookup("__dyld_get_image_vmaddr_slide", (void **) &my_dyld_get_image_vmaddr_slide) ||
	    !_dyld_func_lookup("__dyld_get_image_name", (void **) &my_dyld_get_image_name) ||
	    !_dyld_func_lookup("__dyld_image_count", (void **) &my_dyld_image_count)) {
		panic("dyld_func_lookup failure\n");
	}
}

/* 'dlhand' keeps the image alive */
static bool find_image_hdr_and_slide(const char *filename, const void **hdr, ssize_t *slide, void **dlhand) {
	/* this is just for the refcount; maybe unnecessary for current APIs */
	*dlhand = dlopen(filename, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
	if (!*dlhand)
		return false;

	pthread_once(&dyld_inspect_once, inspect_dyld);
	libsystem_helpers->acquireGlobalDyldLock();
	for (uint32_t i = 0, cnt = my_dyld_image_count(); i < cnt; i++) {
		const char *name = my_dyld_get_image_name(i);
		printf("%s < %s \n", name, filename);
		if (!strcmp(name, filename)) {
			*hdr = my_dyld_get_image_header(i);
			*slide = my_dyld_get_image_vmaddr_slide(i);
			libsystem_helpers->releaseGlobalDyldLock();
			return true;
		}
	}
	panic("%s: found in dlopen but not _dyld_get_image_name\n", __func__);
}

int substitute_find_syms(const char *filename, const char **names,
                         substitute_sym **syms, size_t count) {
	const void *hdr;
	ssize_t slide;
	void *dlhand;
	if (!find_image_hdr_and_slide(filename, &hdr, &slide, &dlhand))
		return SUBSTITUTE_ERR_MODULE_NOT_FOUND;
	substitute_find_syms_raw(hdr, &slide, names, syms, count);
	dlclose(dlhand);
	return SUBSTITUTE_OK;
}

#endif
