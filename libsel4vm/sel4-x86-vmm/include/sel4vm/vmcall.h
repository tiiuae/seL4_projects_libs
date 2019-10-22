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

#pragma once

#include <sel4vm/guest_vm.h>

/*
    Simple functions for registering handlers,
        calling a handler
*/
int reg_new_handler(vm_t *vm, vmcall_handler func, int token);
int vmm_vmcall_handler(vm_vcpu_t *vcpu);

/* Handlers that can be registered */
int vchan_handler(vm_vcpu_t *vcpu);
