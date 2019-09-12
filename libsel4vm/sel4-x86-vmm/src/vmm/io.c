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

/*vm exits related with io instructions*/

#include <stdio.h>
#include <stdlib.h>

#include <sel4/sel4.h>
#include <sel4utils/util.h>
#include <simple/simple.h>

#include <sel4vm/guest_vm.h>
#include "sel4vm/debug.h"
#include "sel4vm/io.h"
#include "sel4vm/vmm.h"

#include "vm.h"

int vm_enable_passthrough_ioport(vm_vcpu_t *vcpu, uint16_t port_start, uint16_t port_end) {
    cspacepath_t path;
    int error;
    ZF_LOGD("Enabling IO port 0x%x - 0x%x for passthrough", port_start, port_end);
    error = vka_cspace_alloc_path(vcpu->vm->vka, &path);
    if (error) {
        ZF_LOGE("Failed to allocate slot");
        return error;
    }
    error = simple_get_IOPort_cap(vcpu->vm->simple, port_start, port_end, path.root, path.capPtr, path.capDepth);
    if (error) {
        ZF_LOGE("Failed to get io port from simple for range 0x%x - 0x%x", port_start, port_end);
        return error;
    }
    error = seL4_X86_VCPU_EnableIOPort(vcpu->vcpu.cptr, path.capPtr, port_start, port_end);
    if (error != seL4_NoError) {
        ZF_LOGE("Failed to enable io port");
        return error;
    }
    return 0;
}

static void set_io_in_unhandled(vm_vcpu_t *vcpu, unsigned int size) {
    uint32_t eax;
    if (size < 4) {
        eax = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EAX);
        eax |= MASK(size * 8);
    } else {
        eax = -1;
    }
    vmm_set_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EAX, eax);
}

static void set_io_in_value(vm_vcpu_t *vcpu, unsigned int value, unsigned int size) {
    uint32_t eax;
    if (size < 4) {
        eax = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EAX);
        eax &= ~MASK(size * 8);
        eax |= value;
    } else {
        eax = value;
    }
    vmm_set_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EAX, eax);
}

/* IO instruction execution handler. */
int vmm_io_instruction_handler(vm_vcpu_t *vcpu) {

    unsigned int exit_qualification = vmm_guest_exit_get_qualification(&vcpu->vcpu_arch.guest_state);
    unsigned int string, rep;
    int ret;
    unsigned int port_no;
    unsigned int size;
    unsigned int value;
    int is_in;

    string = (exit_qualification & 16) != 0;
    is_in = (exit_qualification & 8) != 0;
    port_no = exit_qualification >> 16;
    size = (exit_qualification & 7) + 1;
    rep = (exit_qualification & 0x20) >> 5;

    /*FIXME: does not support string and rep instructions*/
    if (string || rep) {
        ZF_LOGE("FIXME: IO exit does not support string and rep instructions");
        return VM_EXIT_HANDLE_ERROR;
    }

    if (!vcpu->vm->arch.ioport_callback) {
        if (port_no != -1) {
            ZF_LOGW("ignoring unsupported ioport 0x%x", port_no);
        }
        if (is_in) {
            set_io_in_unhandled(vcpu, size);
        }
        vmm_guest_exit_next_instruction(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
        return VM_EXIT_HANDLED;
    }

    if (!is_in) {
        value = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EAX);
        if (size < 4) {
            value &= MASK(size * 8);
        }
    }
    ioport_fault_result_t res = vcpu->vm->arch.ioport_callback(vcpu->vm, port_no, is_in, &value, size, vcpu->vm->arch.ioport_callback_cookie);
    if (is_in) {
        if (res == IO_FAULT_UNHANDLED) {
            set_io_in_unhandled(vcpu, size);
        } else {
            set_io_in_value(vcpu, value, size);
        }
    }

    if (res == IO_FAULT_ERROR) {
        ZF_LOGE("VM Exit IO Error: string %d  in %d rep %d  port no 0x%x size %d", 0,
                is_in, 0, port_no, size);
        return VM_EXIT_HANDLE_ERROR;
    }

    vmm_guest_exit_next_instruction(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);

    return VM_EXIT_HANDLED;
}
