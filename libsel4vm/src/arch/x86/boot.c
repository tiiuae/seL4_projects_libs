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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <utils/util.h>
#include <sel4/sel4.h>
#include <vka/object.h>
#include <vka/capops.h>
#include <sel4utils/mapping.h>
#include <sel4utils/api.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_ram.h>

#include <sel4vm/boot.h>
#include <sel4vm/guest_memory.h>
#include <sel4vm/guest_state.h>
#include <sel4vm/processor/decode.h>
#include <sel4vm/processor/apicdef.h>
#include <sel4vm/processor/lapic.h>
#include <sel4vm/processor/platfeature.h>

#include "vm_boot.h"
#include "guest_vspace.h"

#define VMM_VMCS_CR0_MASK           (X86_CR0_PG | X86_CR0_PE)
#define VMM_VMCS_CR0_VALUE          VMM_VMCS_CR0_MASK
/* We need to own the PSE and PAE bits up until the guest has actually turned on paging,
 * then it can control them
 */
#define VMM_VMCS_CR4_MASK           (X86_CR4_PSE | X86_CR4_PAE | X86_CR4_VMXE)
#define VMM_VMCS_CR4_VALUE          (X86_CR4_PSE | X86_CR4_VMXE)

static int make_guest_page_dir_continued(vm_t *vm, uintptr_t guest_phys, void *vaddr, size_t size, size_t offset, void *cookie) {
    assert(offset == 0);
    assert(size == BIT(seL4_PageBits));
    /* Write into this frame as the init page directory: 4M pages, 1 to 1 mapping. */
    uint32_t *pd = vaddr;
    for (int i = 0; i < 1024; i++) {
        /* Present, write, user, page size 4M */
        pd[i] = (i << PAGE_BITS_4M) | 0x87;
    }
    return 0;
}

static int make_guest_page_dir(vm_t *vm) {
    /* Create a 4K Page to be our 1-1 pd */
    /* This is constructed with magical new memory that we will not tell Linux about */
    uintptr_t pd = (uintptr_t)vspace_new_pages(&vm->mem.vm_vspace, seL4_AllRights, 1, seL4_PageBits);
    if (pd == 0) {
        ZF_LOGE("Failed to allocate page for initial guest pd");
        return -1;
    }
    printf("Guest page dir allocated at 0x%x. Creating 1-1 entries\n", (unsigned int)pd);
    vm->arch.guest_pd = pd;
    return vm_ram_touch(vm, pd, BIT(seL4_PageBits), make_guest_page_dir_continued, NULL);
}

int
vm_init_arch(vm_t *vm, void *cookie) {
    int err;

    if (!vm) {
        ZF_LOGE("Failed to initialise vm arch: Invalid vm");
        return -1;
    }

    vm->arch.vmcall_handlers = NULL;
    vm->arch.vmcall_num_handlers = 0;

    /* Create an EPT which is the pd for all the vcpu tcbs */
    err = vka_alloc_ept_pml4(vm->vka, &vm->mem.vm_vspace_root);
    if (err) {
        return -1;
    }
    /* Assign an ASID */
    err = simple_ASIDPool_assign(vm->simple, vm->mem.vm_vspace_root.cptr);
    if (err != seL4_NoError) {
        ZF_LOGE("Failed to assign ASID pool to EPT root");
        return -1;
    }
    /* Install the guest PD */
    err = seL4_TCB_SetEPTRoot(simple_get_tcb(vm->simple), vm->mem.vm_vspace_root.cptr);
    assert(err == seL4_NoError);
    /* Initialize a vspace for the guest */
    err = vm_init_guest_vspace(&vm->mem.vmm_vspace, &vm->mem.vmm_vspace,
            &vm->mem.vm_vspace, vm->vka, vm->mem.vm_vspace_root.cptr);
    if (err) {
        return err;
    }
    /* Create our 4K page 1-1 pd */
    err = make_guest_page_dir(vm);
    if (err) {
        return -1;
    }
    /* Add local apic handler */
    vm_memory_reservation_t * apic_reservation = vm_reserve_memory_at(vm, APIC_DEFAULT_PHYS_BASE,
            sizeof(struct local_apic_regs), apic_fault_callback, NULL);
    if (!apic_reservation) {
        ZF_LOGE("Failed to reserve apic memory");
        return -1;
    }
    /* ====== To be removed: will be refactored/removed ====== */
    err = vmm_pci_init(&vm->arch.pci);
    if (err) {
        ZF_LOGF("Failed to initialise VM PCI");
    }
    /* Init ioports */
    err = vmm_io_port_init(&vm->arch.io_port);
    if (err) {
        return err;
    }
    err = vmm_mmio_init(&vm->arch.mmio_list);
    if (err) {
        return err;
    }
    /* ======================================================= */

    /* Bind our interrupt pending callback */
    seL4_CPtr notification = vm->callbacks.get_async_event_notification();
    err = seL4_TCB_BindNotification(simple_get_init_cap(vm->simple, seL4_CapInitThreadTCB), vm->callbacks.get_async_event_notification());
    assert(err == seL4_NoError);
    vm->arch.callback_notification = notification;
    return err;
}

int
vm_create_vcpu_arch(vm_t *vm, void* cookie, vm_vcpu_t *vcpu) {
    int err;
    err = seL4_X86_VCPU_SetTCB(vcpu->vcpu.cptr, simple_get_tcb(vm->simple));
    assert(err == seL4_NoError);
    /* All LAPICs are created enabled, in virtual wire mode */
    vmm_create_lapic(vcpu, 1);
    /* Set the initial CR state */
    vcpu->vcpu_arch.guest_state.virt.cr.cr0_mask = VMM_VMCS_CR0_MASK;
    vcpu->vcpu_arch.guest_state.virt.cr.cr0_shadow = 0;
    vcpu->vcpu_arch.guest_state.virt.cr.cr0_host_bits = VMM_VMCS_CR0_VALUE;
    vcpu->vcpu_arch.guest_state.virt.cr.cr4_mask = VMM_VMCS_CR4_MASK;
    vcpu->vcpu_arch.guest_state.virt.cr.cr4_shadow = 0;
    vcpu->vcpu_arch.guest_state.virt.cr.cr4_host_bits = VMM_VMCS_CR4_VALUE;
    /* Set the initial CR states */
    vmm_guest_state_set_cr0(&vcpu->vcpu_arch.guest_state, vcpu->vcpu_arch.guest_state.virt.cr.cr0_host_bits);
    vmm_guest_state_set_cr3(&vcpu->vcpu_arch.guest_state, vm->arch.guest_pd);
    vmm_guest_state_set_cr4(&vcpu->vcpu_arch.guest_state, vcpu->vcpu_arch.guest_state.virt.cr.cr4_host_bits);
    /* Init guest OS vcpu state. */
    vmm_vmcs_init_guest(vcpu);
    return 0;
}
