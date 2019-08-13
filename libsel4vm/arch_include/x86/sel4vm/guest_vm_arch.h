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

#include <sel4vm/driver/pci.h>


/* ================ To be removed: will be refactored/removed ================ */
/* Stores informatoin about the guest image we are loading. This information probably stops
 * being relevant / useful after we start running. Most of this assumes
 * we are loading a guest kernel elf image and that we are acting as some kind of bootloader */
typedef struct guest_image {
    /* Alignment we used when loading the kernel image */
    size_t alignment;
    /* Entry point when the VM starts */
    uintptr_t entry;
    /* Base address (in guest physical) where the image was loaded */
    uintptr_t load_paddr;
    /* Base physical address the image was linked for */
    uintptr_t link_paddr;
    uintptr_t link_vaddr;
    /* If we are loading a guest elf then we may not have been able to put it where it
     * requested. This is the relocation offset */
    int relocation_offset;
    /* Guest physical address of where we stashed the kernel cmd line */
    uintptr_t cmd_line;
    size_t cmd_line_len;
    /* Guest physical address of where we created the boot information */
    uintptr_t boot_info;
    /* Boot module information */
    uintptr_t boot_module_paddr;
    size_t boot_module_size;
} guest_image_t;
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

struct vm_arch {
    /* Exit handler hooks */
    vmexit_handler_ptr vmexit_handlers[VMM_EXIT_REASON_NUM];
    /* VM Call handlers */
    vmcall_handler_t *vmcall_handlers;
    unsigned int vmcall_num_handlers;
    /* Callback notification */
    seL4_CPtr callback_notification;
    /* Guest physical address of where we built the vm's page directory */
    uintptr_t guest_pd;
    /* ====== To be removed: will be refactored/removed ====== */
    vmm_pci_space_t pci;
    vmm_io_port_list_t io_port;
    guest_image_t guest_image;
    /* ======================================================= */
};

struct vm_vcpu_arch {
    /* Records vpcu context */
    guest_state_t guest_state;
    /* VM local apic */
    vmm_lapic_t *lapic;
};
