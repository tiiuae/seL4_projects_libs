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
#include <sel4vm/guest_vm_exits.h>

#include <sel4vm/boot.h>
#include <sel4vm/guest_memory.h>
#include <sel4vm/guest_memory_util.h>
#include <sel4vm/processor/decode.h>
#include <sel4vm/processor/apicdef.h>
#include <sel4vm/processor/lapic.h>
#include <sel4vm/processor/platfeature.h>

#include "vm_boot.h"
#include "guest_vspace.h"
#include "guest_memory_map.h"
#include "guest_state.h"
#include "vmcs.h"

#define VMM_VMCS_CR0_MASK           (X86_CR0_PG | X86_CR0_PE)
#define VMM_VMCS_CR0_VALUE          VMM_VMCS_CR0_MASK
/* We need to own the PSE and PAE bits up until the guest has actually turned on paging,
 * then it can control them
 */
#define VMM_VMCS_CR4_MASK           (X86_CR4_PSE | X86_CR4_PAE | X86_CR4_VMXE)
#define VMM_VMCS_CR4_VALUE          (X86_CR4_PSE | X86_CR4_VMXE)

#define GUEST_PAGE_DIR 0x10000000

static int make_guest_page_dir_continued(void *access_addr, void *vaddr, void *cookie) {
    /* Write into this frame as the init page directory: 4M pages, 1 to 1 mapping. */
    uint32_t *pd = vaddr;
    for (int i = 0; i < 1024; i++) {
        /* Present, write, user, page size 4M */
        pd[i] = (i << PAGE_BITS_4M) | 0x87;
    }
    return 0;
}

static vm_frame_t pd_alloc_iterator(uintptr_t addr, void *cookie) {
    int ret;
    vka_object_t object;
    vm_frame_t frame_result = { seL4_CapNull, seL4_NoRights, 0, 0 };
    vm_t *vm = (vm_t *)cookie;
    if (!vm) {
        return frame_result;
    }
    int page_size = vm->mem.page_size;
    uintptr_t frame_start = ROUND_DOWN(addr, BIT(page_size));
    ret = vka_alloc_frame_maybe_device(vm->vka, page_size, false, &object);
    if (ret) {
        ZF_LOGE("Failed to allocate frame for address 0x%x", (unsigned int)addr);
        return frame_result;
    }
    frame_result.cptr = object.cptr;
    frame_result.rights = seL4_AllRights;
    frame_result.vaddr = frame_start;
    frame_result.size_bits = page_size;
    return frame_result;
}


static int make_guest_page_dir(vm_t *vm) {
    /* Create a 4K Page to be our 1-1 pd */
    /* This is constructed with magical new memory that we will not tell Linux about */
    vm_memory_reservation_t *pd_reservation = vm_reserve_memory_at(vm, GUEST_PAGE_DIR, BIT(seL4_PageBits),
            default_error_fault_callback, NULL);
    if (!pd_reservation) {
        ZF_LOGE("Failed to reserve page for initial guest pd");
        return -1;
    }
    int err = map_vm_memory_reservation(vm, pd_reservation, pd_alloc_iterator, (void *)vm);
    if (err) {
        ZF_LOGE("Failed to map page for initial guest pd");
    }
    printf("Guest page dir allocated at 0x%x. Creating 1-1 entries\n", (unsigned int)GUEST_PAGE_DIR);
    vm->arch.guest_pd = GUEST_PAGE_DIR;
    return vspace_access_page_with_callback(&vm->mem.vm_vspace, &vm->mem.vmm_vspace, (void *)GUEST_PAGE_DIR,
                seL4_PageBits, seL4_AllRights, 1, make_guest_page_dir_continued, NULL);
}

int
vm_init_arch(vm_t *vm, void *cookie) {
    int err;

    if (!vm) {
        ZF_LOGE("Failed to initialise vm arch: Invalid vm");
        return -1;
    }

    /* Initialise x86 specific fields */
    struct vm_init_x86_config *vm_init_x86_params = cookie;
    vm->arch.notification_cap = vm_init_x86_params->notification_cap;

    vm->arch.vmcall_handlers = NULL;
    vm->arch.vmcall_num_handlers = 0;
    vm->arch.ioport_list.num_ioports = 0;
    vm->arch.ioport_list.ioports = NULL;

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

    /* Bind our interrupt pending callback */
    err = seL4_TCB_BindNotification(simple_get_init_cap(vm->simple, seL4_CapInitThreadTCB), vm->arch.notification_cap);
    assert(err == seL4_NoError);
    return err;
}

int
vm_create_vcpu_arch(vm_t *vm, void* cookie, vm_vcpu_t *vcpu) {
    int err;
    err = seL4_X86_VCPU_SetTCB(vcpu->vcpu.cptr, simple_get_tcb(vm->simple));
    assert(err == seL4_NoError);
    /* All LAPICs are created enabled, in virtual wire mode */
    vmm_create_lapic(vcpu, 1);
    vcpu->vcpu_arch.guest_state = calloc(1, sizeof(guest_state_t));
    if (!vcpu->vcpu_arch.guest_state) {
        return -1;
    }
    /* Set the initial CR state */
    vcpu->vcpu_arch.guest_state->virt.cr.cr0_mask = VMM_VMCS_CR0_MASK;
    vcpu->vcpu_arch.guest_state->virt.cr.cr0_shadow = 0;
    vcpu->vcpu_arch.guest_state->virt.cr.cr0_host_bits = VMM_VMCS_CR0_VALUE;
    vcpu->vcpu_arch.guest_state->virt.cr.cr4_mask = VMM_VMCS_CR4_MASK;
    vcpu->vcpu_arch.guest_state->virt.cr.cr4_shadow = 0;
    vcpu->vcpu_arch.guest_state->virt.cr.cr4_host_bits = VMM_VMCS_CR4_VALUE;
    /* Set the initial CR states */
    vmm_guest_state_set_cr0(vcpu->vcpu_arch.guest_state, vcpu->vcpu_arch.guest_state->virt.cr.cr0_host_bits);
    vmm_guest_state_set_cr3(vcpu->vcpu_arch.guest_state, vm->arch.guest_pd);
    vmm_guest_state_set_cr4(vcpu->vcpu_arch.guest_state, vcpu->vcpu_arch.guest_state->virt.cr.cr4_host_bits);
    /* Init guest OS vcpu state. */
    vmm_vmcs_init_guest(vcpu);
    return 0;
}
