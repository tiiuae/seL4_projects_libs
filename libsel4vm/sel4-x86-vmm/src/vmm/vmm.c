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
#include <sel4vm/guest_x86_context.h>
#include <sel4vm/boot.h>
#include "sel4vm/debug.h"
#include "sel4vm/vmm.h"
#include "sel4vm/interrupt.h"

#include "guest_state.h"

void vmm_reply_vm_exit(vm_vcpu_t *vcpu) {
    assert(vcpu->vcpu_arch.guest_state->exit.in_exit);

    if (IS_MACHINE_STATE_MODIFIED(vcpu->vcpu_arch.guest_state->machine.context)) {
        vm_sync_guest_context(vcpu);
    }

    /* Before we resume the guest, ensure there is no dirty state around */
    assert(vmm_guest_state_no_modified(vcpu->vcpu_arch.guest_state));
    vmm_guest_state_invalidate_all(vcpu->vcpu_arch.guest_state);

    vcpu->vcpu_arch.guest_state->exit.in_exit = 0;
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
