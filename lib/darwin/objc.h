#pragma once
/* PAGE_SIZE is not actually a constant on iOS */
#if defined(__arm64__)
#define _PAGE_SHIFT 14
#else
#define _PAGE_SHIFT 12
#endif
#define _PAGE_SIZE (1 << _PAGE_SHIFT)
#if defined(__x86_64__)
#define TRAMPOLINE_SIZE 0x27
#elif defined(__i386__)
#define TRAMPOLINE_SIZE 0x19
#elif defined(__arm__)
#define TRAMPOLINE_SIZE 0x18
#elif defined(__arm64__)
#define TRAMPOLINE_SIZE 0x40
#endif
#ifdef __LP64__
#define TRAMP_INFO_PAGE_ENTRY_SIZE 24
#else
#define TRAMP_INFO_PAGE_ENTRY_SIZE 12
#endif
#define TRAMPOLINES_PER_PAGE (_PAGE_SIZE / TRAMPOLINE_SIZE)
