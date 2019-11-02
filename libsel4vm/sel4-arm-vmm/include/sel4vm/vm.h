/*
 * Copyright 2017, Data61
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

#include <sel4vm/sel4_arch/vm.h>

#include <sel4utils/process.h>
#include <platsupport/io.h>

#include <elf/elf.h>
#include <simple/simple.h>
#include <vspace/vspace.h>
#include <vka/vka.h>

#include <sel4vm/fault.h>

#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
#include <sel4vm/vchan_vm_component.h>
#include <sel4vchan/vchan_component.h>
#endif //CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT

typedef struct vm vm_t;
typedef int (*reboot_hook_fn)(vm_t* vm, void *token);

typedef struct virq_handle *virq_handle_t;

/**
 * Handle a VM event
 * @param[in] vm   A handle to the VM that triggered the event
 * @param[in] tag  The tag of the incomming message
 * @return     0 on success, otherwise, the VM should be shut down
 */
int vm_event(vm_t *vm, seL4_MessageInfo_t tag);

/**
 * Register or replace a virtual IRQ definition
 * @param[in] virq  The IRQ number to inject
 * @param[in] ack   A function to call when the VM ACKs the IRQ
 * @param[in] token A token to pass, unmodified, to the ACK callback function
 */
virq_handle_t vm_virq_new(vm_t *vm, int virq, void (*ack)(void *), void *token);

/**
 * Inject an IRQ into a VM
 * @param[in] virq  A handle to the virtual IRQ
 * @return          0 on success
 */
int vm_inject_IRQ(virq_handle_t virq);

/**
 * Given a guest intermiate physical address, find the actual physical address
 * of the mapping.
 * @param[in] vm       The VM in question
 * @param[in] ipa      The guest physical address in question
 * @param[in] size     The size of the request
 * @return             The physical address which corresponds to the provided
 *                     guest physical address or 0 on error. An error will occur
 *                     If a valid mapping is not present, or if the mapping does
 *                     not provide a contiguous region of the requested size.
 */
uintptr_t vm_ipa_to_pa(vm_t *vm, uintptr_t ipa, size_t size);
