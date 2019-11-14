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

typedef int (*vcpu_exception_handler_fn)(vm_vcpu_t *vcpu, uint32_t hsr);

/**
 * Handle a vcpu exception given the HSR value - Syndrome information
 * The handler can be used such that an ARM VMM can either hook it into
 * the unhandled vcpu callback interface or wrap over it
 * @param[in] vcpu      A handle to the VCPU
 * @param[in] hsr       Syndrome information value describing the exception/fault
 */
int vmm_handle_arm_vcpu_exception(vm_vcpu_t *vcpu, uint32_t hsr, void *cookie);

/**
 * Register a handler to a vcpu exception class
 * @param[in] ec_class              The exception class the handler will be called on
 * @param[in] exception_handler     Pointer to the exception handler
 */
int register_arm_vcpu_exception_handler(uint32_t ec_class, vcpu_exception_handler_fn exception_handler);
