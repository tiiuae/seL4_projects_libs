/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <sel4utils/mapping.h>
#include "vm.h"
#include "fault.h"
#include <stdlib.h>

#include <sel4arm-vmm/devices.h>
#include <sel4arm-vmm/exynos/devices.h>
#include <vka/capops.h>

//#define DEBUG_MAPPINGS

#ifdef DEBUG_MAPPINGS
#define DMAP(...) printf(__VA_ARGS__)
#else
#define DMAP(...) do{}while(0)
#endif


void*
map_device(vspace_t *vspace, vka_t* vka, simple_t* simple, uintptr_t paddr,
           uintptr_t _vaddr, seL4_CapRights rights)
{
    cspacepath_t frame;
    void* vaddr;
    int err;

    paddr &= ~0xfff;
    vaddr = (void*)(_vaddr &= ~0xfff);

    /* Alocate a slot */
    err = vka_cspace_alloc_path(vka, &frame);
    assert(!err);
    if (err) {
        printf("Failed to allocate cslot\n");
        return NULL;
    }

    /* Find the device cap */
    err = simple_get_frame_cap(simple, (void*)paddr, 12, &frame);
    if (err) {
        printf("Failed to find device cap for 0x%x\n", (uint32_t)paddr);
        //vka_cspace_free(vka, frame.capPtr);
        return NULL;
    }
    /* Map the device */
    if (vaddr) {
        reservation_t* res;
        res = vspace_reserve_range_at(vspace, vaddr, 0x1000, rights, 0);
        assert(res);
        if (!res) {
            printf("Failed to reserve vspace\n");
            vka_cspace_free(vka, frame.capPtr);
            return NULL;
        }
        /* Map in the page */
        err = vspace_map_pages_at_vaddr(vspace, &frame.capPtr, vaddr,
                                        1, 12, res);
        vspace_free_reservation(vspace, res);
    } else {
        vaddr = vspace_map_pages(vspace, &frame.capPtr, rights, 1, 12, 0);
        err = (vaddr == 0);
    }
    assert(!err);
    if (err) {
        printf("Failed to provide memory\n");
        vka_cspace_free(vka, frame.capPtr);
        return NULL;
    }
    DMAP("Mapped device ipa0x%x->p0x%x\n", (uint32_t)vaddr, (uint32_t)paddr);
    return vaddr;
}

void*
map_vm_device(vm_t* vm, uintptr_t pa, uintptr_t va, seL4_CapRights rights)
{
    return map_device(vm_get_vspace(vm), vm->vka, vm->simple, pa, va, rights);
}

void*
map_emulated_device(vm_t* vm, const struct device *d)
{
    cspacepath_t vm_frame, vmm_frame;
    vspace_t *vm_vspace, *vmm_vspace;
    void* vm_addr, *vmm_addr;
    reservation_t* res;
    vka_object_t frame;
    vka_t* vka;
    size_t size;
    int err;

    vka = vm->vka;
    vm_addr = (void*)d->pstart;
    size = d->size;
    vm_vspace = vm_get_vspace(vm);
    vmm_vspace = vm->vmm_vspace;
    assert(size == 0x1000);

    /* Create a frame (and a copy for the VMM) */
    err = vka_alloc_frame(vka, 12, &frame);
    assert(!err);
    if (err) {
        return NULL;
    }
    vka_cspace_make_path(vka, frame.cptr, &vm_frame);
    err = vka_cspace_alloc_path(vka, &vmm_frame);
    assert(!err);
    if (err) {
        vka_free_object(vka, &frame);
        return NULL;
    }
    err = vka_cnode_copy(&vmm_frame, &vm_frame, seL4_AllRights);
    assert(!err);
    if (err) {
        vka_cspace_free(vka, vm_frame.capPtr);
        vka_free_object(vka, &frame);
        return NULL;
    }

    /* Map the frame to the VM */
    DMAP("Mapping emulated device ipa0x%x\n", (uint32_t)vm_addr);
    res = vspace_reserve_range_at(vm_vspace, vm_addr, size, seL4_CanRead, 0);
    assert(res);
    if (!res) {
        vka_cspace_free(vka, vm_frame.capPtr);
        vka_cspace_free(vka, vmm_frame.capPtr);
        vka_free_object(vka, &frame);
        return NULL;
    }
    err = vspace_map_pages_at_vaddr(vm_vspace, &vm_frame.capPtr, vm_addr,
                                    1, 12, res);
    vspace_free_reservation(vm_vspace, res);
    assert(!err);
    if (err) {
        printf("Failed to provide memory\n");
        vka_cspace_free(vka, vm_frame.capPtr);
        vka_cspace_free(vka, vmm_frame.capPtr);
        vka_free_object(vka, &frame);
        return NULL;
    }
    vmm_addr = vspace_map_pages(vmm_vspace, &vmm_frame.capPtr, seL4_AllRights,
                                1, 12, 0);
    assert(vmm_addr);
    if (vmm_addr == NULL) {
        return NULL;
    }

    return vmm_addr;
}

void*
map_ram(vspace_t *vspace, vka_t* vka, uintptr_t vaddr)
{
    vka_object_t frame;
    reservation_t* res;
    void* addr;
    int err;

    addr = (void*)(vaddr & ~0xfff);

    /* reserve vspace */
    res = vspace_reserve_range_at(vspace, addr, 0x1000, seL4_AllRights, 1);
    if (!res) {
        assert(res);
        return NULL;
    }
    /* Create a frame */
    err = vka_alloc_frame(vka, 12, &frame);
    assert(!err);
    if (err) {
        vspace_free_reservation(vspace, res);
        return NULL;
    }
    /* Map in the frame */
    err = vspace_map_pages_at_vaddr(vspace, &frame.cptr, addr, 1, 12, res);
    vspace_free_reservation(vspace, res);
    assert(!err);
    if (err) {
        printf("Failed to provide memory\n");
        vka_free_object(vka, &frame);
        return NULL;
    }
    return addr;
}

void*
map_vm_ram(vm_t* vm, uintptr_t vaddr)
{
    return map_ram(vm_get_vspace(vm), vm->vka, vaddr);
}

int
vm_install_passthrough_device(vm_t* vm, const struct device* device)
{
    struct device d;
    uintptr_t paddr;
    int err;
    d = *device;
    for (paddr = d.pstart; paddr - d.pstart < d.size; paddr += 0x1000) {
        void* addr;
        addr = map_vm_device(vm, paddr, paddr, seL4_AllRights);
        if (addr == NULL && paddr == MCT_ADDR) {
            printf("*****************************************\n");
            printf("*** Linux will try to use the MCT but ***\n");
            printf("*** the kernel is not exporting it!   ***\n");
            printf("*****************************************\n");
            /* VMCT is not fully functional yet */
//            err = vm_install_vmct(vm);
            return -1;
        }
        assert(addr);
        if (!addr) {
            return -1;
        }
    }
    err = vm_add_device(vm, &d);
    assert(!err);
    return err;
}

static int
handle_listening_fault(struct device* d, vm_t* vm,
                       fault_t* fault)
{
    volatile uint32_t *reg;
    int offset;

    assert(d->priv);
    offset = fault->addr - d->pstart;

    reg = (volatile uint32_t*)(d->priv + offset);

    printf("Listener: <%s> ", d->name);
    if (fault_is_read(fault)) {
        printf("read");
        fault->data = *reg;
    } else {
        printf("write");
        *reg = fault_emulate(fault, *reg);
    }
    printf(" ");
    fault_print_data(fault);
    printf(" address 0x%08x @ pc 0x%08x\n", fault->addr, fault->regs.pc);
    return advance_fault(fault);
}


int
vm_install_listening_device(vm_t* vm, const struct device* dev_listening)
{
    struct device d;
    int err;
    assert(dev_listening->size == 0x1000);
    d = *dev_listening;
    d.handle_page_fault = handle_listening_fault;
    /* Map locally */
    d.priv = map_device(vm->vmm_vspace, vm->vka, vm->simple,
                        d.pstart, 0, seL4_AllRights);
    assert(d.priv);
    if (!d.priv) {
        return -1;
    }
    err = vm_add_device(vm, &d);
    assert(!err);
    return err;
}


static int
handle_listening_ram_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    volatile uint32_t *reg;
    int offset;

    assert(d->priv);
    offset = fault->addr - d->pstart;

    reg = (volatile uint32_t*)(d->priv + offset);

    if (fault_is_read(fault)) {
        fault->data = *reg;
        printf("Listener pc0x%x| r0x%x:0x%x\n", fault->regs.pc,
               fault->addr,
               fault->data);
    } else {
        printf("Listener pc0x%x| w0x%x:0x%x\n", fault->regs.pc,
               fault->addr,
               fault->data);
        *reg = fault_emulate(fault, *reg);
    }
    return advance_fault(fault);
}


const struct device dev_listening_ram = {
    .devid = DEV_CUSTOM,
    .name = "<listing_ram>",
    .pstart = 0x0,
    .size = 0x1000,
    .handle_page_fault = handle_listening_ram_fault,
    .priv = NULL
};


int
vm_install_listening_ram(vm_t* vm, uintptr_t addr, size_t size)
{
    struct device d;
    int err;
    d = dev_listening_ram;
    d.pstart = addr;
    d.size = size;
    d.priv = malloc(0x1000);
    assert(d.priv);
    if (!d.priv) {
        printf("malloc failed\n");
        return -1;
    }
    err = vm_add_device(vm, &d);
    assert(!err);
    if (err) {
        printf("alloc failed\n");
    }
    return err;
}

