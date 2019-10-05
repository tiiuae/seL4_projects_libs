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

#include <sel4/sel4.h>
#include <sel4/messages.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_vm_util.h>
#include <sel4vm/boot.h>
#include <sel4vm/guest_vm_exits.h>

#include "vm.h"
#include "arm_vm.h"
#include "arm_vm_exits.h"

#include "vgic/vgic.h"
#include "syscalls.h"
#include "mem_abort.h"

static int vm_user_exception_handler(vm_vcpu_t *vcpu);
static int vm_vcpu_handler(vm_vcpu_t *vcpu);
static int vm_unknown_exit_handler(vm_vcpu_t *vcpu);

static vm_exit_handler_fn_t arm_exit_handlers[] = {
    [VM_GUEST_ABORT_EXIT] = vm_guest_mem_abort_handler,
    [VM_SYSCALL_EXIT] = vm_syscall_handler,
    [VM_USER_EXCEPTION_EXIT] = vm_user_exception_handler,
    [VM_VGIC_MAINTENANCE_EXIT] = vm_vgic_maintenance_handler,
    [VM_VCPU_EXIT] = vm_vcpu_handler,
    [VM_UNKNOWN_EXIT] = vm_unknown_exit_handler
};

static int
vm_decode_exit(seL4_Word label)
{
    int exit_reason = VM_UNKNOWN_EXIT;

    switch (label) {
    case seL4_Fault_VMFault:
        exit_reason = VM_GUEST_ABORT_EXIT;
        break;
    case seL4_Fault_UnknownSyscall:
        exit_reason = VM_SYSCALL_EXIT;
        break;
    case seL4_Fault_UserException:
        exit_reason = VM_USER_EXCEPTION_EXIT;
        break;
    case seL4_Fault_VGICMaintenance:
        exit_reason = VM_VGIC_MAINTENANCE_EXIT;
        break;
    case seL4_Fault_VCPUFault:
        exit_reason = VM_VCPU_EXIT;
        break;
    default:
       exit_reason = VM_UNKNOWN_EXIT;
    }
    return exit_reason;
}

static int handle_exception(vm_t* vm, seL4_Word ip)
{
    seL4_UserContext regs;
    seL4_CPtr tcb = vm_get_tcb(vm);
    int err;
    ZF_LOGE("%sInvalid instruction from [%s] at PC: 0x"XFMT"%s\n",
           ANSI_COLOR(RED, BOLD), vm->vm_name, seL4_GetMR(0), ANSI_COLOR(RESET));
    err = seL4_TCB_ReadRegisters(tcb, false, 0, sizeof(regs) / sizeof(regs.pc), &regs);
    assert(!err);
    print_ctx_regs(&regs);
    return VM_EXIT_HANDLED;
}

static int vm_user_exception_handler(vm_vcpu_t *vcpu) {
    seL4_Word ip;
    int err;
    ip = seL4_GetMR(0);
    err = handle_exception(vcpu->vm, ip);
    assert(!err);
    if (!err) {
        seL4_MessageInfo_t reply;
        reply = seL4_MessageInfo_new(0, 0, 0, 0);
        seL4_Reply(reply);
    }
    return VM_EXIT_HANDLED;
}

static int vm_vcpu_handler(vm_vcpu_t *vcpu) {
    uint32_t hsr;
    int err;
    fault_t* fault;
    fault = vcpu->vcpu_arch.fault;
    hsr = seL4_GetMR(seL4_UnknownSyscall_ARG0);
    /* check if the exception class (bits 26-31) of the HSR indicate WFI/WFE */
    if ( (hsr >> HSR_CLASS_EXCEPTION_SHIFT) == HSR_WFI) {
        /* generate a new WFI fault */
        new_wfi_fault(fault);
        return VM_EXIT_HANDLED;
    } else {
        ZF_LOGE("Unhandled VCPU fault from [%s]: HSR 0x%08x\n", vcpu->vm->vm_name, hsr);
        if ((hsr & 0xfc300000) == 0x60200000 || hsr == 0xf2000800) {
            seL4_UserContext *regs;
            new_wfi_fault(fault);
            regs = fault_get_ctx(fault);
            regs->pc += 4;
            seL4_TCB_WriteRegisters(vm_get_tcb(vcpu->vm), false, 0,
                    sizeof(*regs) / sizeof(regs->pc), regs);
            restart_fault(fault);
            return VM_EXIT_HANDLED;
            }
        return VM_EXIT_HANDLE_ERROR;
    }
}

static int vm_unknown_exit_handler(vm_vcpu_t *vcpu) {
    /* What? Why are we here? What just happened? */
    ZF_LOGE("Unknown fault from [%s]", vcpu->vm->vm_name);
    vcpu->vm->run.exit_reason = VM_GUEST_UNKNOWN_EXIT;
    return VM_EXIT_HANDLE_ERROR;
}

static int
vm_stop(vm_t *vm) {
    vm->tcb.is_suspended = true;
    return seL4_TCB_Suspend(vm_get_tcb(vm));
}

static int
vm_resume(vm_t *vm) {
    vm->tcb.is_suspended = false;
    return seL4_TCB_Resume(vm_get_tcb(vm));
}

int vm_run_arch(vm_t *vm) {
    int err;
    int ret;

    err = vm_resume(vm);
    if (err) {
        ZF_LOGE("Failed to start VM");
        return -1;
    }

    /* Loop, handling events */
    while (ret > 0) {
        seL4_MessageInfo_t tag;
        seL4_Word sender_badge;
        seL4_Word label;
        int vm_exit_reason;

        tag = seL4_Recv(vm->arch.fault_endpoint, &sender_badge);
        label = seL4_MessageInfo_get_label(tag);
        if (sender_badge == VM_BADGE) {
            vm_exit_reason = vm_decode_exit(label);
            ret = arm_exit_handlers[vm_exit_reason](vm->vcpus[BOOT_VCPU]);
            if (ret == VM_EXIT_HANDLE_ERROR) {
                vm->run.exit_reason = VM_GUEST_ERROR_EXIT;
            }
        } else {
            if (vm->run.notification_callback) {
                err = vm->run.notification_callback(vm, sender_badge, tag,
                        vm->run.notification_callback_cookie);
            } else {
                ZF_LOGE("Unable to handle VM notification. Exiting");
                err = -1;
            }
            if (err) {
                ret = -1;
                vm->run.exit_reason = VM_GUEST_ERROR_EXIT;
            }
        }
    }

    return ret;
}
