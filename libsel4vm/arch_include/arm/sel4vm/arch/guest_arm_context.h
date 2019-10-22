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

#include <sel4/sel4.h>
#include <sel4vm/guest_vm.h>

/* VCPU Thread Context Getters and Setters */

/**
 * Set a VCPU's thread registers given a TCB user context
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] context   seL4_UserContext applied to VCPU's TCB
 * @return              0 on success, otherwise -1 for error
 */
int vm_set_thread_context(vm_vcpu_t *vcpu, seL4_UserContext context);

/**
 * Set a single VCPU's TCB register
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] reg       Index offset of register in seL4_UserContext e.g pc (seL4_UserContext.pc) => 0
 * @param[in] value     Value to set TCB register with
 * @return              0 on success, otherwise -1 for error
 */
int vm_set_thread_context_reg(vm_vcpu_t *vcpu, unsigned int reg, uintptr_t value);

/**
 * Get a VCPU's TCB user context
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] context   Pointer to user supplied seL4_UserContext to populate with VCPU's TCB user context
 * @return              0 on success, otherwise -1 for error
 */
int vm_get_thread_context(vm_vcpu_t *vcpu, seL4_UserContext *context);

/**
 * Get a single VCPU's TCB register
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] reg       Index offset of register in seL4_UserContext e.g pc (seL4_UserContext.pc) => 0
 * @param[in] value     Pointer to user supplied variable to populate TCB register value with
 * @return              0 on success, otherwise -1 for error
 */
int vm_get_thread_context_reg(vm_vcpu_t *vcpu, unsigned int reg, uintptr_t *value);

/* ARM VCPU Register Getters and Setters */

/**
 * Set an ARM VCPU register
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] reg       VCPU Register field defined in seL4_VCPUReg
 * @param[in] value     Value to set VCPU register with
 * @return              0 on success, otherwise -1 for error
 */
int vm_set_arm_vcpu_reg(vm_vcpu_t *vcpu, seL4_Word reg, uintptr_t value);

/**
 * Get an ARM VCPU register
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] reg       VCPU Register field defined in seL4_VCPUReg
 * @param[in] value     Pointer to user supplied variable to populate VCPU register value with
 * @return              0 on success, otherwise -1 for error
 */
int vm_get_arm_vcpu_reg(vm_vcpu_t *vcpu, seL4_Word reg, uintptr_t *value);
