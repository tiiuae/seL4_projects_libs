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

#include <autoconf.h>
#include <stdio.h>
#include <stdlib.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/boot.h>
#include "sel4vm/guest_memory.h"
#include <sel4vm/sel4_arch/vm.h>
#include <sel4vm/fault.h>
#include <sel4vm/devices.h>

#include "vm.h"
#include "mem_abort.h"

static int unhandled_memory_fault(vm_t *vm, fault_t* fault) {
    uintptr_t addr = fault_get_address(fault);
    size_t fault_size = fault_get_width_size(fault);
    bool is_read_fault = fault_is_read(fault);
    seL4_Word fault_data = 0;

    if (!is_read_fault) {
        fault_data = fault_get_data(fault);
    }
    memory_fault_result_t fault_result = vm->mem.unhandled_mem_fault_handler(vm, addr, fault_size,
            fault_is_read(fault), &fault_data, fault_get_data_mask(fault),
            vm->mem.unhandled_mem_fault_cookie);
     switch(fault_result) {
        case FAULT_HANDLED:
            if (is_read_fault) {
                fault_set_data(fault, fault_data);
                advance_fault(fault);
            }
            return 0;
        case FAULT_RESTART:
            restart_fault(fault);
            return 0;
        case FAULT_IGNORE:
            return ignore_fault(fault);
        case FAULT_ERROR:
            print_fault(fault);
            abandon_fault(fault);
            return -1;
        default:
            break;
    }
    return -1;
}

static inline int dev_paddr_in_range(uintptr_t addr, const struct device* d)
{
    return ( (addr >= d->pstart) && addr < (d->pstart + d->size) );
}

static int cmp_id(struct device* d, void* data)
{
    return !(d->devid == *((enum devid*)data));
}

static int cmp_ipa(struct device* d, void* data)
{
    return !dev_paddr_in_range(*(uintptr_t*)data, d);
}

struct device*
vm_find_device(vm_t* vm, int (*cmp)(struct device* d, void* data), void* data) {
    struct device *ret;
    int i;
    for (i = 0, ret = vm->arch.devices; i < vm->arch.ndevices; i++, ret++) {
        if (cmp(ret, data) == 0) {
            return ret;
        }
    }
    return NULL;
}

struct device*
vm_find_device_by_id(vm_t* vm, enum devid id) {
    return vm_find_device(vm, &cmp_id, &id);
}

struct device*
vm_find_device_by_ipa(vm_t* vm, uintptr_t ipa) {
    return vm_find_device(vm, &cmp_ipa, &ipa);
}

int
handle_page_fault(vm_t* vm, fault_t* fault)
{
    int err;
    struct device* d;
    guest_memory_arch_data_t arch_data;
    uintptr_t addr = fault_get_address(fault);
    size_t fault_size = fault_get_width_size(fault);

    arch_data.fault = fault;
    memory_fault_result_t fault_result = vm_memory_handle_fault(vm, addr, fault_size, arch_data);
    switch(fault_result) {
        case FAULT_HANDLED:
            return 0;
        case FAULT_RESTART:
            restart_fault(fault);
            return 0;
        case FAULT_IGNORE:
            return ignore_fault(fault);
        case FAULT_ERROR:
            print_fault(fault);
            abandon_fault(fault);
            return -1;
        case FAULT_UNHANDLED:
            if (vm->mem.unhandled_mem_fault_handler) {
                err = unhandled_memory_fault(vm, fault);
                if (err) {
                    return -1;
                }
                return 0;
            }
        default:
            break;
            /* We don't have a memory reservation for the faulting address
             * We move onto the rest of the page fault handler */
    }

    /* See if the device is already in our address space */
    d = vm_find_device_by_ipa(vm, fault_get_address(fault));
    if (d != NULL) {
        if (d->devid == DEV_RAM) {
            ZF_LOGI("[%s] %s fault @ 0x%x from 0x%x\n", d->name,
                      (fault_is_read(fault)) ? "read" : "write",
                      fault_get_address(fault), fault_get_ctx(fault)->pc);
        } else {
            ZF_LOGI("[%s] %s fault @ 0x%x from 0x%x\n", d->name,
                      (fault_is_read(fault)) ? "read" : "write",
                      fault_get_address(fault), fault_get_ctx(fault)->pc);
        }
        return d->handle_page_fault(d, vm, fault);
    }
    print_fault(fault);
    abandon_fault(fault);
    return -1;
}

int vm_guest_mem_abort_handler(vm_vcpu_t *vcpu) {
    int err;
    fault_t* fault;
    fault = vcpu->vcpu_arch.fault;
    err = new_fault(fault);
    if (err) {
        ZF_LOGE("Failed to initialise new fault");
        return -1;
    }
    err = handle_page_fault(vcpu->vm, fault);
    if (err) {
        return VM_EXIT_HANDLE_ERROR;
    }
    return VM_EXIT_HANDLED;
}
