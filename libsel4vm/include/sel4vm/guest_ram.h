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
