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

#include <sel4vmmplatsupport/guest_image.h>

void vmm_plat_init_guest_boot_structure(vm_t *vm, const char *cmdline,
        guest_kernel_image_t guest_kernel_image, guest_image_t guest_ramdisk_image);
void vmm_init_guest_thread_state(vm_vcpu_t *vcpu, uintptr_t guest_entry_addr);

