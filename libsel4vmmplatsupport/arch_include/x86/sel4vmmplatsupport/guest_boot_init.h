/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#pragma once

#include <stdint.h>

void vmm_plat_init_guest_boot_structure(vm_t *vm, const char *cmdline,
        uintptr_t guest_kernel_load_addr, size_t guest_kernel_alignment,
        uintptr_t guest_ramdisk_load_addr, size_t guest_ramdisk_size);
void vmm_init_guest_thread_state(vm_vcpu_t *vcpu, uintptr_t guest_entry_addr);

