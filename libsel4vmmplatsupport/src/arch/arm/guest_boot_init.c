/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <sel4vm/guest_vm.h>
#include <sel4vm/boot.h>
#include <sel4vm/arch/guest_arm_context.h>

#include "guest_boot_sel4arch.h"

int vm_set_bootargs(vm_t *vm, seL4_Word pc, seL4_Word mach_type, seL4_Word atags)
{
    seL4_UserContext regs;
    if (!vm) {
        ZF_LOGE("Failed to set bootargs: NULL VM handle");
        return -1;
    }
    sel4arch_set_bootargs(&regs, pc, mach_type, atags);

    /* Write VCPU thread registers */
    int err = vm_set_thread_context(vm->vcpus[BOOT_VCPU], regs);
    if (err) {
        ZF_LOGE("Failed to set VCPU thread context");
    }
    return err;
}
