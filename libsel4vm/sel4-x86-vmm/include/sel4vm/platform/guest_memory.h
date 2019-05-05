/*
 * Copyright 2017, Data61
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
#include <vspace/vspace.h>

struct vmm;

uintptr_t guest_ram_largest_free_region_start(guest_memory_t *guest_memory);
void print_guest_ram_regions(guest_memory_t *guest_memory);
void guest_ram_mark_allocated(guest_memory_t *guest_memory, uintptr_t start, size_t bytes);
uintptr_t guest_ram_allocate(guest_memory_t *guest_memory, size_t bytes);

int vmm_alloc_guest_device_at(struct vmm *vmm, uintptr_t start, size_t bytes);
uintptr_t vmm_map_guest_device(struct vmm *vmm, uintptr_t paddr, size_t bytes, size_t align);
int vmm_map_guest_device_at(struct vmm *vmm, uintptr_t vaddr, uintptr_t paddr, size_t bytes);
int vmm_alloc_guest_ram_at(struct vmm *vmm, uintptr_t start, size_t bytes);
int vmm_alloc_guest_ram(struct vmm *vmm, size_t bytes, int onetoone);

