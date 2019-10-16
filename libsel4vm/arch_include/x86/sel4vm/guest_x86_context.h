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

typedef enum vcpu_context_reg {
    VCPU_CONTEXT_EAX = 0,
    VCPU_CONTEXT_EBX,
    VCPU_CONTEXT_ECX,
    VCPU_CONTEXT_EDX,
    VCPU_CONTEXT_ESI,
    VCPU_CONTEXT_EDI,
    VCPU_CONTEXT_EBP
} vcpu_context_reg_t;

/* VCPU Thread Context Getters and Setters */

/**
 * Set a VCPU's thread registers given a seL4_VCPUContext
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] context   seL4_VCPUContext applied to VCPU Registers
 * @return              0 on success, otherwise -1 for error
 */
int vm_set_thread_context(vm_vcpu_t *vcpu, seL4_VCPUContext context);

/**
 * Set a single VCPU's thread register in a seL4_VCPUContext
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] reg       Register enumerated by vcpu_context_reg
 * @param[in] value     Value to set register with
 * @return              0 on success, otherwise -1 for error
 */
int vm_set_thread_context_reg(vm_vcpu_t *vcpu, vcpu_context_reg_t reg, uint32_t value);

/**
 * Get a VCPU's thread context
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] context   Pointer to user supplied seL4_VCPUContext to populate with VCPU's current context
 * @return              0 on success, otherwise -1 for error
 */
int vm_get_thread_context(vm_vcpu_t *vcpu, seL4_VCPUContext *context);

/**
 * Get a single VCPU's thread register
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] reg       Register enumerated by vcpu_context_reg
 * @param[in] value     Pointer to user supplied variable to populate register value with
 * @return              0 on success, otherwise -1 for error
 */
int vm_get_thread_context_reg(vm_vcpu_t *vcpu, vcpu_context_reg_t reg, uint32_t *value);

/* VMCS Getters and Setters */

/**
 * Set a VMCS field
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] reg       VMCS field
 * @param[in] value     Value to set VMCS field with
 * @return              0 on success, otherwise -1 for error
 */
int vm_set_vmcs_field(vm_vcpu_t *vcpu, seL4_Word field, uint32_t value);

/**
 * Get a VMCS register
 * @param[in] vcpu      Handle to the vcpu
 * @param[in] reg       VMCS fiedl
 * @param[in] value     Pointer to user supplied variable to populate VMCS field value with
 * @return              0 on success, otherwise -1 for error
 */
int vm_get_vmcs_field(vm_vcpu_t *vcpu, seL4_Word field, uint32_t *value);
