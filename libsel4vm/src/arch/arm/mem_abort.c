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

#include "mem_abort.h"

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

static int alloc_vm_device_cap(uintptr_t addr, vm_t* vm, vm_frame_t *frame_result) {
    int err;
    cspacepath_t frame;
    err = vka_cspace_alloc_path(vm->vka, &frame);
    if (err) {
        ZF_LOGE("Failed to allocate cslot\n");
        return -1;
    }
    seL4_Word cookie;
    err = vka_utspace_alloc_at(vm->vka, &frame, kobject_get_type(KOBJECT_FRAME, 12), 12, addr, &cookie);
    if (err) {
        err = simple_get_frame_cap(vm->simple, (void*)addr, 12, &frame);
        if (err) {
            return -1;
        }
    }
    frame_result->cptr = frame.capPtr;
    frame_result->rights = seL4_AllRights;
    frame_result->vaddr = addr;
    frame_result->size_bits = vm->mem.page_size;
    return 0;
}

static int alloc_vm_ram_cap(uintptr_t addr, vm_t* vm, vm_frame_t *frame_result) {
    int err;
    cspacepath_t frame;
    vka_object_t frame_obj;
    err = vka_alloc_frame_maybe_device(vm->vka, 12, true, &frame_obj);
    if (err) {
        ZF_LOGF("Failed vka_alloc_frame_maybe_device");
        return -1;
    }
    vka_cspace_make_path(vm->vka, frame_obj.cptr, &frame);
    frame_result->cptr = frame.capPtr;
    frame_result->rights = seL4_AllRights;
    frame_result->vaddr = addr;
    frame_result->size_bits = vm->mem.page_size;
    return 0;
}

static vm_frame_t on_demand_iterator(uintptr_t addr, void *cookie) {
    int err;
    uintptr_t paddr = addr & ~0xfff;
    vm_frame_t frame_result = { seL4_CapNull, seL4_NoRights, 0, 0 };
    vm_t *vm = (vm_t *)cookie;
    /* Attempt allocating device memory */
    err = alloc_vm_device_cap(paddr, vm, &frame_result);
    if (!err) {
        return frame_result;
    }
    /* Attempt allocating ram memory */
    err = alloc_vm_ram_cap(paddr, vm, &frame_result);
    if (err) {
        ZF_LOGE("Failed to create on demand memory for addr 0x%x", addr);
    }
    return frame_result;
}

static memory_fault_result_t
handle_on_demand_fault_callback(vm_t *vm, uintptr_t fault_addr, size_t fault_length,
        void *cookie, guest_memory_arch_data_t arch_data) {
    ZF_LOGE("Fault for on demand memory region: 0x%x", fault_addr);
    return FAULT_ERROR;
}

int
handle_page_fault(vm_t* vm, fault_t* fault)
{
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
        default: /* FAULT_UNHANDLED */
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
    } else {
#ifdef CONFIG_ONDEMAND_DEVICE_INSTALL
        uintptr_t addr = fault_get_address(fault) & ~0xfff;
        int mapped;
        vm_memory_reservation_t *reservation;
        switch (addr) {
        case 0:
            print_fault(fault);
            return -1;
        default:
            reservation = vm_reserve_memory_at(vm, addr, 0x1000,
                    handle_on_demand_fault_callback, NULL);
            mapped = vm_map_reservation(vm, reservation, on_demand_iterator, (void *)vm);
            if (!mapped) {
                restart_fault(fault);
                return 0;
            }
            ZF_LOGW("Unhandled fault on address 0x%x\n", (uint32_t)addr);
        }
#endif
        print_fault(fault);
        abandon_fault(fault);
        return -1;
    }
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
    return err;
}
