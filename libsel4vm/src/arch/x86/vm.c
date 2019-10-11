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
#include <string.h>

#include <sel4/sel4.h>
#include <sel4/messages.h>
#include <platsupport/arch/tsc.h>
#include <sel4/arch/vmenter.h>
#include <vka/capops.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_vm_util.h>
#include <sel4vm/boot.h>
#include <sel4vm/guest_vm_exits.h>

#include "vm.h"
#include "i8259/i8259.h"

#include "sel4vm/debug.h"
#include "sel4vm/vmm.h"
#include "sel4vm/interrupt.h"
#include "sel4vm/platform/boot_guest.h"

static vm_exit_handler_fn_t x86_exit_handlers[] = {
    [EXIT_REASON_PENDING_INTERRUPT] = vmm_pending_interrupt_handler,
    [EXIT_REASON_CPUID] = vmm_cpuid_handler,
    [EXIT_REASON_MSR_READ] = vmm_rdmsr_handler,
    [EXIT_REASON_MSR_WRITE] = vmm_wrmsr_handler,
    [EXIT_REASON_EPT_VIOLATION] = vmm_ept_violation_handler,
    [EXIT_REASON_CR_ACCESS] = vmm_cr_access_handler,
    [EXIT_REASON_IO_INSTRUCTION] = vmm_io_instruction_handler,
    [EXIT_REASON_HLT] = vmm_hlt_handler,
    [EXIT_REASON_VMX_TIMER] = vmm_vmx_timer_handler,
    [EXIT_REASON_VMCALL] = vmm_vmcall_handler,
};

static void vm_resume(vm_vcpu_t *vcpu) {
    vmm_sync_guest_state(vcpu);
    if (vcpu->vcpu_arch.guest_state.exit.in_exit && !vcpu->vcpu_arch.guest_state.virt.interrupt_halt) {
        /* Guest is blocked, but we are no longer halted. Reply to it */
        vmm_reply_vm_exit(vcpu);
    }
}

/* Handle VM exit in VMM module. */
static int handle_vm_exit(vm_vcpu_t *vcpu) {
    int ret;
    int reason = vmm_guest_exit_get_reason(&vcpu->vcpu_arch.guest_state);

    /* Distribute the task according to the exit info. */
    vmm_print_guest_context(3, vcpu);

    if (reason == -1) {
        ZF_LOGF("Kernel failed to perform vmlaunch or vmresume, we have no recourse");
    }

    if (!x86_exit_handlers[reason]) {
        printf("VM_FATAL_ERROR ::: vm exit handler is NULL for reason 0x%x.\n", reason);
        vmm_print_guest_context(0, vcpu);
        vcpu->vcpu_online = false;
        return -1;
    }

    /* Call the handler. */
    ret = x86_exit_handlers[reason](vcpu);
    if (ret == -1) {
        printf("VM_FATAL_ERROR ::: vmexit handler return error\n");
        vmm_print_guest_context(0, vcpu);
        vcpu->vcpu_online = false;
        return ret;
    }

    /* Reply to the VM exit exception to resume guest. */
    vm_resume(vcpu);
    return ret;
}

static void vm_update_guest_state_from_interrupt(vm_vcpu_t *vcpu, seL4_Word *msg) {
    vcpu->vcpu_arch.guest_state.machine.eip = msg[SEL4_VMENTER_CALL_EIP_MR];
    vcpu->vcpu_arch.guest_state.machine.control_ppc = msg[SEL4_VMENTER_CALL_CONTROL_PPC_MR];
    vcpu->vcpu_arch.guest_state.machine.control_entry = msg[SEL4_VMENTER_CALL_CONTROL_ENTRY_MR];
}

static void vm_update_guest_state_from_fault(vm_vcpu_t *vcpu, seL4_Word *msg) {
    assert(vcpu->vcpu_arch.guest_state.exit.in_exit);

    /* The interrupt state is a subset of the fault state */
    vm_update_guest_state_from_interrupt(vcpu, msg);

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

int vm_run_arch(vm_t *vm) {
    int err;
    int ret;
    vm_vcpu_t *vcpu = vm->vcpus[BOOT_VCPU];

    vcpu->vcpu_arch.guest_state.virt.interrupt_halt = 0;
    vcpu->vcpu_arch.guest_state.exit.in_exit = 0;

    /* Sync the existing guest state */
    vmm_sync_guest_state(vcpu);
    vmm_sync_guest_context(vcpu);
    /* Now invalidate everything */
    assert(vmm_guest_state_no_modified(&vcpu->vcpu_arch.guest_state));
    vmm_guest_state_invalidate_all(&vcpu->vcpu_arch.guest_state);

    ret = 1;
    vm->run.exit_reason = -1;
    while (ret > 0) {
        /* Block and wait for incoming msg or VM exits. */
        seL4_Word badge;
        int fault;

        if (vcpu->vcpu_online && !vcpu->vcpu_arch.guest_state.virt.interrupt_halt && !vcpu->vcpu_arch.guest_state.exit.in_exit) {
            seL4_SetMR(0, vmm_guest_state_get_eip(&vcpu->vcpu_arch.guest_state));
            seL4_SetMR(1, vmm_guest_state_get_control_ppc(&vcpu->vcpu_arch.guest_state));
            seL4_SetMR(2, vmm_guest_state_get_control_entry(&vcpu->vcpu_arch.guest_state));
            fault = seL4_VMEnter(&badge);

            vmm_guest_state_invalidate_all(&vcpu->vcpu_arch.guest_state);
            if (fault == SEL4_VMENTER_RESULT_FAULT) {
                /* We in a fault */
                vcpu->vcpu_arch.guest_state.exit.in_exit = 1;
                /* Update the guest state from a fault */
                seL4_Word fault_message[SEL4_VMENTER_RESULT_FAULT_LEN];
                for (int i = 0 ; i < SEL4_VMENTER_RESULT_FAULT_LEN; i++) {
                    fault_message[i] = seL4_GetMR(i);
                }
                vm_update_guest_state_from_fault(vcpu, fault_message);
            } else {
                /* update the guest state from a non fault */
                seL4_Word int_message[SEL4_VMENTER_RESULT_NOTIF_LEN];
                for (int i = 0 ; i < SEL4_VMENTER_RESULT_NOTIF_LEN; i++) {
                    int_message[i] = seL4_GetMR(i);
                }
                vm_update_guest_state_from_interrupt(vcpu, int_message);
            }
        } else {
            seL4_Wait(vm->arch.notification_cap, &badge);
            fault = SEL4_VMENTER_RESULT_NOTIF;
        }

        if (fault == SEL4_VMENTER_RESULT_NOTIF) {
            assert(badge >= vm->num_vcpus);
            /* assume interrupt */
            if (vm->run.notification_callback) {
                seL4_MessageInfo_t tag = {0};
                err = vm->run.notification_callback(vm, badge, tag, vm->run.notification_callback_cookie);
                if (err == -1) {
                    ret = VM_EXIT_HANDLE_ERROR;
                } else if (i8259_has_interrupt(vm)) {
                    /* Check if this caused PIC to generate interrupt */
                    vmm_check_external_interrupt(vm);
                }
             } else {
                ZF_LOGE("Unable to handle VM notification. Exiting");
                ret = VM_EXIT_HANDLE_ERROR;
             }
        } else {
            /* Handle the vm exit */
            ret = handle_vm_exit(vcpu);
            vmm_check_external_interrupt(vm);
        }

        if (ret == VM_EXIT_HANDLE_ERROR) {
            vm->run.exit_reason = VM_GUEST_ERROR_EXIT;
        }

    }
    return ret;
}
