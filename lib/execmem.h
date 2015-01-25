#pragma once
#include <stdlib.h>
/* Write to a foreign page which is already RX / with unknown permissions. */
int execmem_write(void *dest, const void *src, size_t len);

/* For allocating trampolines - this is just a mmap wrapper. */
int execmem_alloc_unsealed(uintptr_t hint, void **page_p, size_t *size_p);
int execmem_seal(void *page);
void execmem_free(void *page);
