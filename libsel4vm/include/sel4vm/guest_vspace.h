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

#pragma once

#include <sel4/sel4.h>
#include <vspace/vspace.h>
#include <vka/vka.h>

#include <sel4vm/guest_vspace_arch.h>

/* Constructs a vspace that will duplicate mappings between a page directory and several IO spaces
 * as well as translations to mappings in the VMM vspace  */
int vm_get_guest_vspace(vspace_t *loader, vspace_t *vmm, vspace_t *new_vspace, vka_t *vka, seL4_CPtr page_directory);

/* Callback used for each portion of vmm_guest_vspace_touch.
 * if the return value is non zero this signals the parent loop to stop and return the given value */
typedef int (*vm_guest_vspace_touch_callback)(uintptr_t guest_phys, void *vmm_vaddr, size_t size, size_t offset, seL4_CPtr cap, void *cookie);

/* 'touch' a series of guest physical addresses by invoking the callback function
 * each equivalent range of addresses in the vmm vspace */
int vm_guest_vspace_touch(vspace_t *guest_vspace, uintptr_t addr, size_t size, vm_guest_vspace_touch_callback callback, void *cookie);

/* Helpers for use with vspace touch */
int vm_guest_get_phys_data_help(uintptr_t addr, void *vaddr, size_t size,
        size_t offset, seL4_CPtr cap, void *cookie);

int vm_guest_set_phys_data_help(uintptr_t addr, void *vaddr, size_t size,
        size_t offset, seL4_CPtr cap, void *cookie);

#if defined(CONFIG_ARM_SMMU) || defined(CONFIG_IOMMU)
int vm_guest_vspace_add_iospace(vspace_t *loader, vspace_t *vspace, seL4_CPtr iospace);
#endif
