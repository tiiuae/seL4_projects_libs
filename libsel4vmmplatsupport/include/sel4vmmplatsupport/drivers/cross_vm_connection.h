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

#include <sel4vmmplatsupport/drivers/virtio.h>
#include <sel4vmmplatsupport/drivers/pci_helper.h>
#include <pci/helper.h>

typedef void (*event_callback_fn)(void *arg);
typedef void (*emit_fn)(void);
typedef int (*consume_callback_fn)(event_callback_fn, void *arg);
typedef int (*alloc_free_interrupt_fn)(void);

/* Datastructure representing a dataport of a crossvm connection */
typedef struct crossvm_dataport_handle {
    size_t size;
    unsigned int num_frames;
    seL4_CPtr *frames;
} crossvm_dataport_handle_t;

/* Datastructure representing a single crossvm connection */
typedef struct crossvm_handle {
    crossvm_dataport_handle_t *dataport;
    emit_fn emit_fn;
    seL4_Word consume_id;
} crossvm_handle_t;

/**
 * Install a set of cross vm connections into a guest VM (for either x86 or ARM VM platforms)
 * @param[in] vm                        A handle to the VM
 * @param[in] connection_base_addr      The base guest physical address that can be used to reserve memory
 *                                      for the crossvm connectors
 * @param[in] connections               The set of crossvm connections to be initialised and installed in the guest
 * @param[in] num_connection            The number of connections passed in through the 'connections' parameter
 * @param[in] pci                       A handle to the VM's host PCI device. The connections are advertised through the
 *                                      PCI device
 * @param[in] alloc_irq                 A function that is used to allocated an irq number for the crossvm connections
 * @return                              -1 on failure otherwise 0 for success
 */
int cross_vm_connections_init_common(vm_t *vm, uintptr_t connection_base_addr, crossvm_handle_t *connections,
                                     int num_connections, vmm_pci_space_t *pci, alloc_free_interrupt_fn alloc_irq);

/**
 * Handler to consume a cross vm connection event. This being called by the VMM when it recieves a notification from an
 * external process. The event is then relayed onto the VM.
 * @param[in] vm                        A handle to the VM
 * @param[in] event_id                  The id that corresponds to the occuring event
 * @param[in] inject_irq                Whether to inject an interrupt into the VM
 */
void consume_connection_event(vm_t *vm, seL4_Word event_id, bool inject_irq);
