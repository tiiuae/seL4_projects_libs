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

#include <sel4vm/guest_state.h>
#include <sel4vm/platform/vmexit.h>
#include <sel4vm/processor/lapic.h>

typedef enum ioport_fault_result {
    IO_FAULT_HANDLED,
    IO_FAULT_UNHANDLED,
    IO_FAULT_ERROR
} ioport_fault_result_t;

/* ================ To be removed: will be refactored/removed ================ */
/* Stores informatoin about the guest image we are loading. This information probably stops
 * being relevant / useful after we start running. Most of this assumes
 * we are loading a guest kernel elf image and that we are acting as some kind of bootloader */
typedef struct guest_boot_info {
    /* Guest physical address of where we created the boot information */
    uintptr_t boot_info;
} guest_boot_info_t;
/* ============================================================================ */

/* Function prototype for vm exit handlers */
typedef int(*vmexit_handler_ptr)(vm_vcpu_t *vcpu);

/* A handler for a given vmcall function
 * Called in a vmcall exception if its token matches the vcpu register eax */
typedef int (*vmcall_handler)(vm_vcpu_t *vcpu);
typedef struct vmcall_handler {
    int token;
    vmcall_handler func;
} vmcall_handler_t;

typedef ioport_fault_result_t (*ioport_callback_fn)(vm_t *vm, unsigned int port_no, bool is_in, unsigned int *value,
        size_t size, void *cookie);

struct vm_arch {
    /* Exit handler hooks */
    vmexit_handler_ptr vmexit_handlers[VMM_EXIT_REASON_NUM];
    /* VM Call handlers */
    vmcall_handler_t *vmcall_handlers;
    unsigned int vmcall_num_handlers;
    /* Guest physical address of where we built the vm's page directory */
    uintptr_t guest_pd;
    seL4_CPtr notification_cap;
    ioport_callback_fn ioport_callback;
    void *ioport_callback_cookie;
    /* ====== To be removed: will be refactored/removed ====== */
    guest_boot_info_t guest_boot_info;
    int (*get_interrupt)();
    int (*has_interrupt)();
    /* ======================================================= */
};

struct vm_vcpu_arch {
    /* Records vpcu context */
    guest_state_t guest_state;
    /* VM local apic */
    vmm_lapic_t *lapic;
};

/* IOPort fault callback registration functions */
int vm_register_ioport_callback(vm_t *vm, ioport_callback_fn ioport_callback,
                                      void *cookie);
