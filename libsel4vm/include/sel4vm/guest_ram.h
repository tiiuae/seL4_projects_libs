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

#include <sel4vm/guest_vm.h>

typedef int (*ram_touch_callback_fn)(vm_t *vm, uintptr_t guest_addr, void *vmm_vaddr, size_t size, size_t offset, void *cookie);
int vm_guest_ram_read_callback(vm_t *vm, uintptr_t guest_addr, void *vaddr, size_t size, size_t offset, void *buf);
int vm_guest_ram_write_callback(vm_t *vm, uintptr_t guest_addr, void *vaddr, size_t size, size_t offset, void *buf);

/* Touch a series of pages in the guest vm and invoke a callback for each page accessed
 * @param[in] vm        A handle to the VM
 * @param[in] addr      Address to access in the guest vm
 * @param[in] size      Size of memory region to access
 * @param[in] callback  Callback to invoke on each page access
 * @param[in] cookie    User data to pass onto callback
 */
int vm_ram_touch(vm_t *vm, uintptr_t addr, size_t size, ram_touch_callback_fn touch_callback, void *cookie);

/**
 * Find the largest free ram region
 * @param[in] vm                A handle to the VM
 * @param[in] addr              Pointer to be set with largest region address
 * @param[in] size              Pointer to be set with largest region size
 * @return                     -1 on failure, otherwise 0 for success
 */
int vm_ram_find_largest_free_region(vm_t *vm, uintptr_t *addr, size_t *size);

/**
 * Reserve a region of memory for RAM in the guest VM
 * @param[in] vm                A handle to the VM
 * @param[in] bytes             Size of RAM region to allocate
 * @param[in] one_to_one        Allocate RAM frames such that it has a one-to-one mapping
 *                              between guest PA <-> PA
 * @return                      Starting address of registered ram region
 */
uintptr_t vm_ram_register(vm_t *vm, size_t bytes, bool one_to_one);

/**
 * Reserve a region of memory for RAM in the guest VM at a starting guest physical address
 * @param[in] vm            A handle to the VM that ram needs to be allocated for
 * @param[in] start         Starting guest physical address of the ram region being allocated
 * @param[in] size          The size of the RAM region to be allocated
 * @param[in] untyped       Allocate RAM frames such that it uses untyped memory
 * @return                  0 on success
 */
int vm_ram_register_at(vm_t *vm, uintptr_t start, size_t bytes, bool untyped);

/**
 * Mark a registered region of RAM as allocated
 * @param[in] vm                A handle to the VM
 * @param[in] start             Starting address of guest ram region
 * @param[in] bytes             Size of RAM region
 */
void vm_ram_mark_allocated(vm_t *vm, uintptr_t start, size_t bytes);

/**
 * Allocate a region of registered ram
 * @param[in] vm                A handle to the VM
 * @param[in] bytes             Size of allocation
 * @return                      Starting address of allocated ram region
 */
uintptr_t vm_ram_allocate(vm_t *vm, size_t bytes);

/**
 * Free a RAM a previously allocated RAM region
 * @param[in] vm        A handle to the VM that ram needs to be free'd for
 * @param[in] start     Starting guest physical address of the ram region being free'd
 * @param[in] size      The size of the RAM region to be free'd
 */
void vm_ram_free(vm_t *vm, uintptr_t start, size_t bytes);
