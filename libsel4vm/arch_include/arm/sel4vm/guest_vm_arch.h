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

/* ======= NOTE: Not permanent - will be refactored ======= */
#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
#include <sel4vm/vchan_vm_component.h>
#include <sel4vchan/vchan_component.h>
#endif /*CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT */
/* ======================================================== */

#define VM_CSPACE_SIZE_BITS    4
#define VM_FAULT_EP_SLOT       1
#define VM_CSPACE_SLOT         2

struct vm_arch {
    /* ====== NOTE: Not permanent - will be refactored ====== */
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
