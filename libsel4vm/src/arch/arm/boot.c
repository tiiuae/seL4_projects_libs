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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <utils/util.h>
#include <sel4/sel4.h>
#include <vka/object.h>
#include <vka/capops.h>
#include <sel4utils/mapping.h>
#include <sel4utils/api.h>

#include <sel4vm/boot.h>
#include <sel4vm/guest_vm.h>
#include "sel4vm/guest_vspace.h"

#include <sel4pci/pci.h>

#include "vm_boot.h"

int
vm_init_arch(vm_t *vm, void *cookie) {
    seL4_Word null_cap_data = seL4_NilData;
    seL4_Word cspace_root_data;
    cspacepath_t src, dst;
    vka_t *vka;
    int err;

    /* Check arm specific initialisation parameters */
    if (!cookie) {
        ZF_LOGE("Failed to initialise vm arch: Invalid cookie");
        return -1;
    }
    if (!vm) {
        ZF_LOGE("Failed to initialise vm arch: Invalid vm");
        return -1;
    }
    struct vm_init_arm *vm_init_arm_params = cookie;

    /* Initialise arm specific fields */
    vm->arch.entry_point = NULL;
    vm->arch.fault_endpoint = vm_init_arm_params->vmm_endpoint;

    /* ====== NOTE: Not permanent - will be refactored ====== */
    vm->arch.nhooks = 0;
    err = vmm_pci_init(&vm->arch.pci);
    if (err) {
        ZF_LOGF("Failed to initialise VM PCI");
    }
    err = vmm_io_port_init(&vm->arch.io_port);
    if (err) {
        ZF_LOGE("Failed to initialise VM ioports");
        return err;
    }
    /* ====================================================== */

    /* Create a cspace */
    vka = vm->vka;
    err = vka_alloc_cnode_object(vka, VM_CSPACE_SIZE_BITS, &vm->tcb.cspace);
    assert(!err);
    vka_cspace_make_path(vka, vm->tcb.cspace.cptr, &src);
    cspace_root_data = api_make_guard_skip_word(seL4_WordBits - VM_CSPACE_SIZE_BITS);
    dst.root = vm->tcb.cspace.cptr;
    dst.capPtr = VM_CSPACE_SLOT;
    dst.capDepth = VM_CSPACE_SIZE_BITS;
    err = vka_cnode_mint(&dst, &src, seL4_AllRights, cspace_root_data);
    assert(!err);

    /* Create a vspace */
    err = vka_alloc_vspace_root(vka, &vm->mem.vm_pd);
    assert(!err);
    err = simple_ASIDPool_assign(vm->simple, vm->mem.vm_pd.cptr);
    assert(err == seL4_NoError);
    err = vmm_get_guest_vspace(&vm->mem.vmm_vspace, &vm->mem.vm_vspace, vm->vka, vm->mem.vm_pd.cptr);
    assert(!err);

    /* Badge the endpoint */
    vka_cspace_make_path(vka, vm_init_arm_params->vmm_endpoint, &src);
    err = vka_cspace_alloc_path(vka, &dst);
    assert(!err);
    err = vka_cnode_mint(&dst, &src, seL4_AllRights, vm_init_arm_params->vm_badge);
    assert(!err);
    /* Copy it to the cspace of the VM for fault IPC */
    src = dst;
    dst.root = vm->tcb.cspace.cptr;
    dst.capPtr = VM_FAULT_EP_SLOT;
    dst.capDepth = VM_CSPACE_SIZE_BITS;
    err = vka_cnode_copy(&dst, &src, seL4_AllRights);
    assert(!err);

    /* Create TCB */
    err = vka_alloc_tcb(vka, &vm->tcb.tcb);
    assert(!err);
    err = seL4_TCB_Configure(vm->tcb.tcb.cptr, VM_FAULT_EP_SLOT,
                             vm->tcb.cspace.cptr, cspace_root_data,
                             vm->mem.vm_pd.cptr, null_cap_data, 0, seL4_CapNull);
    assert(!err);
    err = seL4_TCB_SetSchedParams(vm->tcb.tcb.cptr, simple_get_tcb(vm->simple), vm->tcb.priority - 1, vm->tcb.priority - 1);
    assert(!err);

    return err;
}

int
vm_create_vcpu_arch(vm_t *vm, void* cookie, vm_vcpu_t *vcpu) {
    int err;
    err = seL4_ARM_VCPU_SetTCB(vcpu->vm_vcpu.cptr, vm->tcb.tcb.cptr);
    assert(!err);
    vcpu->vcpu_arch.fault = fault_init(vcpu);
    assert(vcpu->vcpu_arch.fault);
    return 0;
}
