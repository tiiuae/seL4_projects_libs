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

#include <sel4vm/arch/vmexit_reasons.h>
#include <sel4vm/arch/ioports.h>

#define IO_APIC_DEFAULT_PHYS_BASE   0xfec00000
#define APIC_DEFAULT_PHYS_BASE      0xfee00000

typedef struct vm_lapic vm_lapic_t;
typedef struct i8259 i8259_t;
typedef struct guest_state guest_state_t;

/* Function prototype for vm exit handlers */
typedef int(*vmexit_handler_ptr)(vm_vcpu_t *vcpu);

/* A handler for a given vmcall function
 * Called in a vmcall exception if its token matches the vcpu register eax */
typedef int (*vmcall_handler)(vm_vcpu_t *vcpu);
typedef struct vmcall_handler {
    int token;
    vmcall_handler func;
} vmcall_handler_t;

struct vm_arch {
    /* Exit handler hooks */
    vmexit_handler_ptr vmexit_handlers[VM_EXIT_REASON_NUM];
    /* VM Call handlers */
    vmcall_handler_t *vmcall_handlers;
    unsigned int vmcall_num_handlers;
    /* Guest physical address of where we built the vm's page directory */
    uintptr_t guest_pd;
    seL4_CPtr notification_cap;
    unhandled_ioport_callback_fn unhandled_ioport_callback;
    void *unhandled_ioport_callback_cookie;
    vm_io_port_list_t ioport_list;
    /* PIC machine state */
    i8259_t *i8259_gs;
};

struct vm_vcpu_arch {
    /* Records vcpu context */
    guest_state_t *guest_state;
    /* VM local apic */
    vm_lapic_t *lapic;
};
