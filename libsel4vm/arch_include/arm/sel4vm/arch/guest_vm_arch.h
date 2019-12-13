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

typedef struct fault fault_t;
typedef struct vm_vcpu vm_vcpu_t;

typedef int (*unhandled_vcpu_fault_callback_fn)(vm_vcpu_t *vcpu, uint32_t hsr, void *cookie);

#define VM_CSPACE_SIZE_BITS    4
#define VM_FAULT_EP_SLOT       1
#define VM_CSPACE_SLOT         2

struct vm_arch {};

struct vm_vcpu_arch {
    fault_t *fault;
    unhandled_vcpu_fault_callback_fn unhandled_vcpu_callback;
    void * unhandled_vcpu_callback_cookie;
};

int vm_register_unhandled_vcpu_fault_callback(vm_vcpu_t *vcpu, unhandled_vcpu_fault_callback_fn vcpu_fault_callback,
                                      void *cookie);
