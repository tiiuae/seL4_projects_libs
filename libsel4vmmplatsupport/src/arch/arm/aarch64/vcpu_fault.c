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

sysreg_entry_t sysreg_table[] = {
};

static bool is_sysreg_match(sysreg_t *sysreg_a, sysreg_t *sysreg_b) {
    return (
        (sysreg_a->params.op0 == sysreg_b->params.op0) &&
        (sysreg_a->params.op1 == sysreg_b->params.op1) &&
        (sysreg_a->params.op2 == sysreg_b->params.op2) &&
        (sysreg_a->params.crn == sysreg_b->params.crn) &&
        (sysreg_a->params.crm == sysreg_b->params.crm)
    );
}

static sysreg_entry_t* find_sysreg_entry(vm_vcpu_t *vcpu, sysreg_t *sysreg_op) {
    for (int i = 0; i < ARRAY_SIZE(sysreg_table); i++) {
        sysreg_entry_t *sysreg_entry = &sysreg_table[i];
        if (is_sysreg_match(sysreg_op, &sysreg_entry->sysreg)) {
            return sysreg_entry;
        }
    }
    return NULL;
}

int sysreg_exception_handler(vm_vcpu_t *vcpu, uint32_t hsr) {
    sysreg_t sysreg_op;
    sysreg_op.hsr_val = hsr;
    sysreg_entry_t *entry = find_sysreg_entry(vcpu, &sysreg_op);
    if (!entry) {
        return -1;
    }
    return entry->handler(vcpu, &entry->sysreg, sysreg_op.params.direction);
}
