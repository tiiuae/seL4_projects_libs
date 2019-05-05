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
#include <sel4vm/fault.h>

#include <sel4pci/pci.h>
#include <sel4vmmcore/util/io.h>

/* ======= NOTE: Not permanent - will be refactored ======= */
#include <sel4vm/devices.h>
#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
#include <sel4vm/vchan_vm_component.h>
#include <sel4vchan/vchan_component.h>
#endif /*CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT */

#define MAX_DEVICES_PER_VM 50
#define MAX_REBOOT_HOOKS_PER_VM 10

typedef int (*reboot_hook_fn)(vm_t* vm, void *token);

struct reboot_hooks {
    reboot_hook_fn fn;
    void *token;
};
/* ======================================================== */

#define VM_CSPACE_SIZE_BITS    4
#define VM_FAULT_EP_SLOT       1
#define VM_CSPACE_SLOT         2

struct vm_arch {
    /* Guest vm entry point */
    void *entry_point;
    /* Guest vm dtb */
    void *dtb;
    /* Endpoint */
    seL4_CPtr fault_endpoint;
    /* ====== NOTE: Not permanent - will be refactored ====== */
    /* Installed reboot hooks */
    struct reboot_hooks rb_hooks[MAX_REBOOT_HOOKS_PER_VM];
    int nhooks;
    /* Installed devices */
    struct device devices[MAX_DEVICES_PER_VM];
    int ndevices;
    /* IOPort Manager */
    vmm_io_port_list_t *io_port;
    /* Virtual PCI Host Bridge */
    vmm_pci_space_t *pci;
#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
    /* Installed vchan connections */
    camkes_vchan_con_t **vchan_cons;
    unsigned int vchan_num_cons;

    int (*lock)(void);
    int (*unlock)(void);
#endif /* CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT */
    /* ====================================================== */
};

struct vm_vcpu_arch {
    fault_t *fault;
};
