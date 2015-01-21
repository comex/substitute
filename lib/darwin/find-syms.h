#pragma once
#include "substitute.h"
#include "substitute-internal.h"

struct symtab_data {
    uint64_t linkedit_vmaddr;
    uint32_t linkedit_symoff, nsyms;
    uint32_t linkedit_stroff, strsize;
};

bool find_symtab_data(const mach_header_x *mh, struct symtab_data *data);
void find_syms_raw(const struct symtab_data *restrict data, void *linkedit,
                   const char **restrict names, substitute_sym **restrict syms,
                   size_t count);
