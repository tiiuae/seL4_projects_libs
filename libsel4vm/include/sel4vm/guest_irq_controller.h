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

/* Callback for irq acknowledgement */
typedef void (*irq_ack_fn_t)(vm_t *vm, int irq, void *cookie);

/**
 * Inject an IRQ into a VM's interrupt controller
 * @param vm    Handle to the VM
 * @param irq   IRQ number to inject
 * @return      0 on success, otherwise -1 for error
 */
int vm_inject_irq(vm_t *vm, int irq);

/**
 * Set level of IRQ number into a VM's interrupt controller
 * @param vm        Handle to the VM
 * @param irq       IRQ number to set level on
 * @param irq_level Value of IRQ level
 * @return          0 on success, otherwise -1 for error
 */
int vm_set_irq_level(vm_t *vm, int irq, int irq_level);

/**
 * Register irq with an acknowledgment function
 * @param vm        Handle to the VM
 * @param irq       IRQ number to register acknowledgement function on
 * @param ack_fn    IRQ acknowledgement function
 * @param cookie    Cookie to pass back with IRQ acknowledgement function
 * @return          0 on success, otherwise -1 for error
 */
int vm_register_irq(vm_t *vm, int irq, irq_ack_fn_t ack_fn, void *cookie);

/**
 * Install the default interrupt controller into the VM
 * @param vm    Handle to the VM
 */
int vm_create_default_irq_controller(vm_t *vm);
