#pragma once

#include <stdio.h>
#include <string.h>

#define substitute_panic(...) do { \
    fprintf(stderr, __VA_ARGS__); \
    abort(); \
    __builtin_unreachable(); \
} while(0)

#define EXPORT __attribute__ ((visibility("default")))
#define UNUSED __attribute__((unused))

#ifdef __APPLE__
#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/nlist.h>
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
/* instead of 'nlist_x', use substitute_sym */
#endif

/* FORCE_* are for tests */
#if defined(FORCE_TARGET_x86_64)
    #define TARGET_x86_64
#elif defined(FORCE_TARGET_i386)
    #define TARGET_i386
#elif defined(FORCE_TARGET_arm)
    #define TARGET_arm
#elif defined(FORCE_TARGET_arm64)
    #define TARGET_arm64
#elif defined(__x86_64__)
    #define TARGET_x86_64
#elif defined(__i386__)
    #define TARGET_i386
#elif defined(__arm__)
    #define TARGET_arm
#elif defined(__arm64__)
    #define TARGET_arm64
#else
    #error target?
#endif

#if defined(TARGET_arm)
    #define TARGET_DIR arm
#elif defined(TARGET_arm64)
    #define TARGET_DIR arm64
#elif defined(TARGET_x86_64) || defined(TARGET_i386)
    #define TARGET_DIR x86
#endif
#define stringify_(x) #x
#define stringify(x) stringify_(x)
#include stringify(TARGET_DIR/misc.h)

#if TARGET_POINTER_SIZE == 8
    typedef uint64_t uint_tptr;
#elif TARGET_POINTER_SIZE == 4
    typedef uint32_t uint_tptr;
#endif


#ifdef __APPLE__
/* This could graduate to a public API but is not yet.  Needs more
 * functionality. */

enum {
    /* substitute_dlopen_in_pid: task_for_pid failed; on OS X the reasons this
     * can happen are really complicated and dumb, but generally one solution
     * is to be root */
    SUBSTITUTE_ERR_TASK_FOR_PID = 1000,
    SUBSTITUTE_ERR_MISC,
};

enum shuttle_type {
    SUBSTITUTE_SHUTTLE_MACH_PORT,
    /* ... */
};

struct shuttle {
    int type;
    union {
        struct {
            mach_port_t port;
            mach_msg_type_name_t right_type;
        } mach;
    } u;
};

int substitute_dlopen_in_pid(int pid, const char *filename, int options,
                             const struct shuttle *shuttle, size_t nshuttle,
                             char **error);

int substitute_ios_unrestrict(task_t task, char **error);
#endif

static const char *xbasename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

