#pragma once
#include <stdint.h>

struct _x86_thread_state_32 {
    uint32_t eax, ebx, ecx, edx, edi, esi, ebp, esp;
    uint32_t ss, eflags, eip, cs, ds, es, fs, gs;
};
#define _x86_thread_state_32_flavor 1
struct _x86_thread_state_64 {
    uint64_t rax, rbx, rcx, rdx, rdi, rsi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, rflags, cs, fs, gs;
};
#define _x86_thread_state_64_flavor 4
struct _arm_thread_state_32 {
    uint32_t r[13], sp, lr, pc, cpsr;
};
#define _arm_thread_state_32_flavor 9
struct _arm_thread_state_64 {
    uint64_t x[29], fp, lr, sp, pc;
    uint32_t cpsr, pad;
};
#define _arm_thread_state_64_flavor 6

kern_return_t mach_vm_read_overwrite(vm_map_t, mach_vm_address_t, mach_vm_size_t, mach_vm_address_t, mach_vm_size_t *);
kern_return_t mach_vm_remap(vm_map_t, mach_vm_address_t *, mach_vm_size_t, mach_vm_offset_t, int, vm_map_t, mach_vm_address_t, boolean_t, vm_prot_t *, vm_prot_t *, vm_inherit_t);
kern_return_t mach_vm_write(vm_map_t, mach_vm_address_t, vm_offset_t, mach_msg_type_number_t);
kern_return_t mach_vm_allocate(vm_map_t, mach_vm_address_t *, mach_vm_size_t, int);
kern_return_t mach_vm_deallocate(vm_map_t, mach_vm_address_t, mach_vm_size_t);
kern_return_t mach_vm_region(vm_map_t, mach_vm_address_t *, mach_vm_size_t *, vm_region_flavor_t, vm_region_info_t, mach_msg_type_number_t *, mach_port_t *);

