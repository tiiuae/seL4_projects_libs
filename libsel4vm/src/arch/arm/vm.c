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

#include "arm_vm.h"
#include "vm.h"

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
    while (1) {
        seL4_MessageInfo_t tag;
        seL4_Word sender_badge;
        seL4_Word label;

        tag = seL4_Recv(vm->arch.fault_endpoint, &sender_badge);
        label = seL4_MessageInfo_get_label(tag);
        if (sender_badge == VM_BADGE) {
            ret = vm_event(vm, tag);
            if (ret) {
                break;
            }
        } else {
            err = vm->callbacks.do_async(sender_badge, label);
        }
    }

    return ret;
}
