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

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_vcpu_fault.h>
#include <sel4vm/arch/processor.h>
#include <sel4vm/arch/guest_arm_context.h>
#include <sel4vm/sel4_arch/processor.h>
#include <sel4vmmplatsupport/arch/guest_vcpu_fault.h>

#include "vcpu_fault_handlers.h"

int software_breakpoint_exception(vm_vcpu_t *vcpu, uint32_t hsr) {
    /* Ignore software breakpoints and just resume execution at the next
     * instruction */
    int err;
    /* TODO: Maybe perform read */
    seL4_UserContext regs;
    err =  vm_get_thread_context(vcpu, &regs);
    if (err) {
        return -1;
    }
    regs.pc += 4;
    err = vm_set_thread_context(vcpu, regs);
    if (err) {
        return -1;
    }
    restart_vcpu_fault(vcpu);
    return 0;
}
