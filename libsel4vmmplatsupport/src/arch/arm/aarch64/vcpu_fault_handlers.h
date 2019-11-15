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

#pragma once

#include <sel4vm/guest_vm.h>
#include <sel4vm/arch/processor.h>
#include <sel4vm/sel4_arch/processor.h>
#include <sel4vmmplatsupport/arch/guest_vcpu_fault.h>

#include "../vcpu_fault.h"

typedef union sysreg {
    uint32_t hsr_val;
    struct sysreg_params {
        uint32_t direction:1;
        uint32_t crm:4;
        uint32_t rt:5;
        uint32_t crn:4;
        uint32_t op1:3;
        uint32_t op2:3;
        uint32_t op0:2;
        uint32_t res0:3;
        uint32_t instr_len:1;
        uint32_t ec:6;
    } params;
} sysreg_t;

typedef int (*sysreg_exception_handler_fn)(vm_vcpu_t *vcpu, sysreg_t *sysreg_reg, bool is_read);

typedef struct sysreg_entry {
    sysreg_t sysreg;
    sysreg_exception_handler_fn handler;
} sysreg_entry_t;

int sysreg_exception_handler(vm_vcpu_t *vcpu, uint32_t hsr);

static vcpu_exception_handler_fn vcpu_exception_handlers[] = {
    [0 ... HSR_MAX_EXCEPTION] = unknown_vcpu_exception_handler,
    [HSR_SYSREG_64_EXCEPTION] = sysreg_exception_handler
};
