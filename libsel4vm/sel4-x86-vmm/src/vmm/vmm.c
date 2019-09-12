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

/* Functions for VMM main host thread.
 *
 *     Authors:
 *         Qian Ge
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sel4/sel4.h>
#include <vka/capops.h>
#include <platsupport/arch/tsc.h>
#include <sel4/arch/vmenter.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/boot.h>
#include "sel4vm/debug.h"
#include "sel4vm/vmm.h"
#include "sel4vm/interrupt.h"
#include "sel4vm/platform/boot_guest.h"

void vmm_sync_guest_context(vm_vcpu_t *vcpu) {
    if (IS_MACHINE_STATE_MODIFIED(vcpu->vcpu_arch.guest_state.machine.context)) {
        seL4_VCPUContext context;
        context.eax = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EAX);
        context.ebx = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EBX);
        context.ecx = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_ECX);
        context.edx = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EDX);
        context.esi = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_ESI);
        context.edi = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EDI);
        context.ebp = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EBP);
        seL4_X86_VCPU_WriteRegisters(vcpu->vcpu.cptr, &context);
        /* Sync our context */
        MACHINE_STATE_SYNC(vcpu->vcpu_arch.guest_state.machine.context);
    }
}

void vmm_reply_vm_exit(vm_vcpu_t *vcpu) {
    assert(vcpu->vcpu_arch.guest_state.exit.in_exit);

    if (IS_MACHINE_STATE_MODIFIED(vcpu->vcpu_arch.guest_state.machine.context)) {
        vmm_sync_guest_context(vcpu);
    }

    /* Before we resume the guest, ensure there is no dirty state around */
    assert(vmm_guest_state_no_modified(&vcpu->vcpu_arch.guest_state));
    vmm_guest_state_invalidate_all(&vcpu->vcpu_arch.guest_state);

    vcpu->vcpu_arch.guest_state.exit.in_exit = 0;
}

void vmm_sync_guest_state(vm_vcpu_t *vcpu) {
    vmm_guest_state_sync_cr0(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
    vmm_guest_state_sync_cr3(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
    vmm_guest_state_sync_cr4(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
    vmm_guest_state_sync_idt_base(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
    vmm_guest_state_sync_idt_limit(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
    vmm_guest_state_sync_gdt_base(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
    vmm_guest_state_sync_gdt_limit(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
    vmm_guest_state_sync_cs_selector(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
    vmm_guest_state_sync_entry_exception_error_code(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
}

int vmm_finalize(vm_t *vm) {
    int err;

    vm_vcpu_t *vcpu = vm->vcpus[BOOT_VCPU];

    vmm_init_guest_thread_state(vcpu);
    return 0;
}

seL4_CPtr vmm_create_async_event_notification_cap(vm_t *vm, seL4_Word badge) {

    if (!(badge & BIT(27))) {
        ZF_LOGE("Invalid badge");
        return seL4_CapNull;
    }

    // notification cap
    seL4_CPtr ntfn = vm->arch.notification_cap;

    // path to notification cap slot
    cspacepath_t ntfn_path;
    vka_cspace_make_path(vm->vka, ntfn, &ntfn_path);

    // allocate slot to store copy
    cspacepath_t minted_ntfn_path = {};
    vka_cspace_alloc_path(vm->vka, &minted_ntfn_path);

    // mint the notification cap
    int error = vka_cnode_mint(&minted_ntfn_path, &ntfn_path, seL4_AllRights, badge);

    if (error != seL4_NoError) {
        ZF_LOGE("Failed to mint notification cap");
        return seL4_CapNull;
    }

    return minted_ntfn_path.capPtr;
}
