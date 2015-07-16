#include "substitute.h"
#include "substitute-internal.h"
#include <syslog.h>
#include <mach-o/dyld.h>

EXPORT
void *SubGetImageByName(const char *filename) __asm__("SubGetImageByName");
void *SubGetImageByName(const char *filename) {
    return substitute_open_image(filename);
}

EXPORT
void *SubFindSymbol(void *image, const char *name) __asm__("SubFindSymbol");
void *SubFindSymbol(void *image, const char *name) {
    if (!image) {
        const char *s = "SubFindSymbol: 'any image' specified, which is incredibly slow - like, 2ms on a fast x86.  I'm going to do it since it seems to be somewhat common, but you should be ashamed of yourself.";
        syslog(LOG_WARNING, "%s", s);
        fprintf(stderr, "%s\n", s);
        /* and it isn't thread safe, but neither is MS */
        for(uint32_t i = 0; i < _dyld_image_count(); i++) {
            const char *im_name = _dyld_get_image_name(i);
            struct substitute_image *im = substitute_open_image(im_name);
            if (!im) {
                fprintf(stderr, "(btw, couldn't open %s?)\n", im_name);
                continue;
            }
            void *r = SubFindSymbol(im, name);
            substitute_close_image(im);
            if (r)
                return r;
        }
        return NULL;
    }

    void *ptr;
    if (substitute_find_private_syms(image, &name, &ptr, 1))
        return NULL;
    return ptr;
}

#ifdef TARGET_DIS_SUPPORTED
EXPORT
void SubHookFunction(void *symbol, void *replace, void **result)
    __asm__("SubHookFunction");
void SubHookFunction(void *symbol, void *replace, void **result) {
    struct substitute_function_hook hook = {symbol, replace, result};
    int ret = substitute_hook_functions(&hook, 1, NULL,
                                        SUBSTITUTE_NO_THREAD_SAFETY);
    if (ret) {
        substitute_panic("SubHookFunction: substitute_hook_functions returned %s\n",
                         substitute_strerror(ret));
    }
}
#endif

EXPORT
void SubHookMessageEx(Class _class, SEL sel, IMP imp, IMP *result)
    __asm__("SubHookMessageEx");

void SubHookMessageEx(Class _class, SEL sel, IMP imp, IMP *result) {
    int ret = substitute_hook_objc_message(_class, sel, imp, result, NULL);
    if (ret) {
        substitute_panic("SubHookMessageEx: substitute_hook_objc_message returned %s\n",
                         substitute_strerror(ret));
    }
}
