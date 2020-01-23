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

#include <sel4/sel4.h>

#include <sel4vm/guest_vm.h>

/**
 * Set the boot args and pc for the VM.
 * For linux:
 *   r0 -> 0
 *   r1 -> MACH_TYPE  (e.g #4151 for EXYNOS5410 eval. platform smdk5410)
 *   r2 -> atags/dtb address
 * @param[in] vm        A handle to a VM
 * @param[in] pc        The initial PC for the VM
 * @param[in] mach_type Linux specific machine ID
 *                      see http://www.arm.linux.org.uk/developer/machines/
 * @param[in] atags     Linux specific IPA of atags. Can also be substituted with dtb address
 * @return              0 on success, otherwise -1 for failure
 */
int vcpu_set_bootargs(vm_vcpu_t *vcpu, seL4_Word pc, seL4_Word mach_type, seL4_Word atags);
