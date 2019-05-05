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
        seL4_X86_VCPU_WriteRegisters(vcpu->vm_vcpu.cptr, &context);
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
    vmm_guest_state_sync_cr0(&vcpu->vcpu_arch.guest_state, vcpu->vm_vcpu.cptr);
    vmm_guest_state_sync_cr3(&vcpu->vcpu_arch.guest_state, vcpu->vm_vcpu.cptr);
    vmm_guest_state_sync_cr4(&vcpu->vcpu_arch.guest_state, vcpu->vm_vcpu.cptr);
    vmm_guest_state_sync_idt_base(&vcpu->vcpu_arch.guest_state, vcpu->vm_vcpu.cptr);
    vmm_guest_state_sync_idt_limit(&vcpu->vcpu_arch.guest_state, vcpu->vm_vcpu.cptr);
    vmm_guest_state_sync_gdt_base(&vcpu->vcpu_arch.guest_state, vcpu->vm_vcpu.cptr);
    vmm_guest_state_sync_gdt_limit(&vcpu->vcpu_arch.guest_state, vcpu->vm_vcpu.cptr);
    vmm_guest_state_sync_cs_selector(&vcpu->vcpu_arch.guest_state, vcpu->vm_vcpu.cptr);
    vmm_guest_state_sync_entry_exception_error_code(&vcpu->vcpu_arch.guest_state, vcpu->vm_vcpu.cptr);
}

/* Handle VM exit in VMM module. */
static void vmm_handle_vm_exit(vm_vcpu_t *vcpu) {
    int reason = vmm_guest_exit_get_reason(&vcpu->vcpu_arch.guest_state);

    /* Distribute the task according to the exit info. */
    vmm_print_guest_context(3, vcpu);

    if (reason == -1) {
        ZF_LOGF("Kernel failed to perform vmlaunch or vmresume, we have no recourse");
    }

    if (!vcpu->vm->arch.vmexit_handlers[reason]) {
        printf("VM_FATAL_ERROR ::: vm exit handler is NULL for reason 0x%x.\n", reason);
        vmm_print_guest_context(0, vcpu);
        vcpu->vcpu_online = 0;
        return;
    }

    /* Call the handler. */
    if (vcpu->vm->arch.vmexit_handlers[reason](vcpu)) {
        printf("VM_FATAL_ERROR ::: vmexit handler return error\n");
        vmm_print_guest_context(0, vcpu);
        vcpu->vcpu_online = 0;
        return;
    }

    /* Reply to the VM exit exception to resume guest. */
    vmm_sync_guest_state(vcpu);
    if (vcpu->vcpu_arch.guest_state.exit.in_exit && !vcpu->vcpu_arch.guest_state.virt.interrupt_halt) {
        /* Guest is blocked, but we are no longer halted. Reply to it */
        vmm_reply_vm_exit(vcpu);
    }
}

static void vmm_update_guest_state_from_interrupt(vm_vcpu_t *vcpu, seL4_Word *msg) {
    vcpu->vcpu_arch.guest_state.machine.eip = msg[SEL4_VMENTER_CALL_EIP_MR];
    vcpu->vcpu_arch.guest_state.machine.control_ppc = msg[SEL4_VMENTER_CALL_CONTROL_PPC_MR];
    vcpu->vcpu_arch.guest_state.machine.control_entry = msg[SEL4_VMENTER_CALL_CONTROL_ENTRY_MR];
}

static void vmm_update_guest_state_from_fault(vm_vcpu_t *vcpu, seL4_Word *msg) {
    assert(vcpu->vcpu_arch.guest_state.exit.in_exit);

    /* The interrupt state is a subset of the fault state */
    vmm_update_guest_state_from_interrupt(vcpu, msg);

    vcpu->vcpu_arch.guest_state.exit.reason = msg[SEL4_VMENTER_FAULT_REASON_MR];
    vcpu->vcpu_arch.guest_state.exit.qualification = msg[SEL4_VMENTER_FAULT_QUALIFICATION_MR];
    vcpu->vcpu_arch.guest_state.exit.instruction_length = msg[SEL4_VMENTER_FAULT_INSTRUCTION_LEN_MR];
    vcpu->vcpu_arch.guest_state.exit.guest_physical = msg[SEL4_VMENTER_FAULT_GUEST_PHYSICAL_MR];

    MACHINE_STATE_READ(vcpu->vcpu_arch.guest_state.machine.rflags, msg[SEL4_VMENTER_FAULT_RFLAGS_MR]);
    MACHINE_STATE_READ(vcpu->vcpu_arch.guest_state.machine.guest_interruptibility, msg[SEL4_VMENTER_FAULT_GUEST_INT_MR]);

    MACHINE_STATE_READ(vcpu->vcpu_arch.guest_state.machine.cr3, msg[SEL4_VMENTER_FAULT_CR3_MR]);

    seL4_VCPUContext context;
    context.eax = msg[SEL4_VMENTER_FAULT_EAX];
    context.ebx = msg[SEL4_VMENTER_FAULT_EBX];
    context.ecx = msg[SEL4_VMENTER_FAULT_ECX];
    context.edx = msg[SEL4_VMENTER_FAULT_EDX];
    context.esi = msg[SEL4_VMENTER_FAULT_ESI];
    context.edi = msg[SEL4_VMENTER_FAULT_EDI];
    context.ebp = msg[SEL4_VMENTER_FAULT_EBP];
    MACHINE_STATE_READ(vcpu->vcpu_arch.guest_state.machine.context, context);
}

/* Entry point of of VMM main host module. */
void vmm_run(vm_t *vm) {
    int UNUSED error;
    DPRINTF(2, "VMM MAIN HOST MODULE STARTED\n");

    for (int i = 0; i < vm->num_vcpus; i++) {
        vm_vcpu_t *vcpu = vm->vcpus[i];

        vcpu->vcpu_arch.guest_state.virt.interrupt_halt = 0;
        vcpu->vcpu_arch.guest_state.exit.in_exit = 0;

        /* sync the existing guest state */
        vmm_sync_guest_state(vcpu);
        vmm_sync_guest_context(vcpu);
        /* now invalidate everything */
        assert(vmm_guest_state_no_modified(&vcpu->vcpu_arch.guest_state));
        vmm_guest_state_invalidate_all(&vcpu->vcpu_arch.guest_state);
    }

    /* Start the boot vcpu guest thread running */
    vm->vcpus[BOOT_VCPU]->vcpu_online = 1;

    while (1) {
        /* Block and wait for incoming msg or VM exits. */
        seL4_Word badge;
        int fault;

        vm_vcpu_t *vcpu = vm->vcpus[BOOT_VCPU];

        if (vcpu->vcpu_online && !vcpu->vcpu_arch.guest_state.virt.interrupt_halt && !vcpu->vcpu_arch.guest_state.exit.in_exit) {
            seL4_SetMR(0, vmm_guest_state_get_eip(&vcpu->vcpu_arch.guest_state));
            seL4_SetMR(1, vmm_guest_state_get_control_ppc(&vcpu->vcpu_arch.guest_state));
            seL4_SetMR(2, vmm_guest_state_get_control_entry(&vcpu->vcpu_arch.guest_state));
            fault = seL4_VMEnter(&badge);

            if (fault == SEL4_VMENTER_RESULT_FAULT) {
                /* We in a fault */
                vcpu->vcpu_arch.guest_state.exit.in_exit = 1;

                /* Update the guest state from a fault */
                seL4_Word fault_message[SEL4_VMENTER_RESULT_FAULT_LEN];
                for (int i = 0 ; i < SEL4_VMENTER_RESULT_FAULT_LEN; i++) {
                    fault_message[i] = seL4_GetMR(i);
                }
                vmm_guest_state_invalidate_all(&vcpu->vcpu_arch.guest_state);
                vmm_update_guest_state_from_fault(vcpu, fault_message);
            } else {
                /* update the guest state from a non fault */
                seL4_Word int_message[SEL4_VMENTER_RESULT_NOTIF_LEN];
                for (int i = 0 ; i < SEL4_VMENTER_RESULT_NOTIF_LEN; i++) {
                    int_message[i] = seL4_GetMR(i);
                }
                vmm_guest_state_invalidate_all(&vcpu->vcpu_arch.guest_state);
                vmm_update_guest_state_from_interrupt(vcpu, int_message);
            }
        } else {
            seL4_Wait(vm->arch.callback_notification, &badge);
            fault = SEL4_VMENTER_RESULT_NOTIF;
        }

        if (fault == SEL4_VMENTER_RESULT_NOTIF) {
            assert(badge >= vm->num_vcpus);
            /* assume interrupt */
            int raise = vm->callbacks.do_async(badge);
            if (raise == 0) {
                /* Check if this caused PIC to generate interrupt */
                vmm_check_external_interrupt(vm);
            }

            continue;
        }

        /* Handle the vm exit */
        vmm_handle_vm_exit(vcpu);

        vmm_check_external_interrupt(vm);

        DPRINTF(5, "VMM main host blocking for another message...\n");
    }

}

static void vmm_exit_init(vm_t *vm) {
    /* Connect VM exit handlers to correct function pointers */
    vm->arch.vmexit_handlers[EXIT_REASON_PENDING_INTERRUPT] = vmm_pending_interrupt_handler;
    //vm->arch.vmexit_handlers[EXIT_REASON_EXCEPTION_NMI] = vmm_exception_handler;
    vm->arch.vmexit_handlers[EXIT_REASON_CPUID] = vmm_cpuid_handler;
    vm->arch.vmexit_handlers[EXIT_REASON_MSR_READ] = vmm_rdmsr_handler;
    vm->arch.vmexit_handlers[EXIT_REASON_MSR_WRITE] = vmm_wrmsr_handler;
    vm->arch.vmexit_handlers[EXIT_REASON_EPT_VIOLATION] = vmm_ept_violation_handler;
    vm->arch.vmexit_handlers[EXIT_REASON_CR_ACCESS] = vmm_cr_access_handler;
    vm->arch.vmexit_handlers[EXIT_REASON_IO_INSTRUCTION] = vmm_io_instruction_handler;
/*    vm->arch.vmexit_handlers[EXIT_REASON_RDTSC] = vmm_rdtsc_instruction_handler;*/
    vm->arch.vmexit_handlers[EXIT_REASON_HLT] = vmm_hlt_handler;
    vm->arch.vmexit_handlers[EXIT_REASON_VMX_TIMER] = vmm_vmx_timer_handler;
    vm->arch.vmexit_handlers[EXIT_REASON_VMCALL] = vmm_vmcall_handler;
}

int vmm_finalize(vm_t *vm) {
    int err;
    vmm_exit_init(vm);

    for (int i = 0; i < vm->num_vcpus; i++) {
        vm_vcpu_t *vcpu = vm->vcpus[i];

        vmm_init_guest_thread_state(vcpu);
        err = vmm_io_port_init_guest(&vm->arch.io_port, vm->simple, vcpu->vm_vcpu.cptr, vm->vka);
        if (err) {
            return err;
        }
    }
    return 0;
}

seL4_CPtr vmm_create_async_event_notification_cap(vm_t *vm, seL4_Word badge) {

    if (!(badge & BIT(27))) {
        ZF_LOGE("Invalid badge");
        return seL4_CapNull;
    }

    // notification cap
    seL4_CPtr ntfn = vm->callbacks.get_async_event_notification();

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
