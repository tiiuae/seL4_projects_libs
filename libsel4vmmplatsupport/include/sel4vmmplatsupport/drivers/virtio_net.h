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

#include <sel4vmmplatsupport/ioports.h>
#include <sel4vmmplatsupport/drivers/pci.h>
#include <sel4vmmplatsupport/drivers/virtio_pci_emul.h>

/* Virtio net driver interface */
typedef struct virtio_net {
    /* IO Port base for Virtio net device */
    unsigned int iobase;
    /* Virtio Ethernet emulation interface: VMM <-> Guest */
    virtio_emul_t *emul;
    /* Backend  Ethernet driver interface: VMM <-> Ethernet driver */
    struct eth_driver *emul_driver;
    /* Virtio Ethernet emulation functions: VMM <-> Guest */
    struct raw_iface_funcs emul_driver_funcs;
    /* ioops for dma management */
    ps_io_ops_t ioops;
} virtio_net_t;

/**
 * Initialise a new virtio_net device with Base Address Registers (BARs) starting at iobase and backend functions
 *
 * specified by the raw_iface_funcs struct.
 * @param vm vm handle
 * @param pci PCI library instance to register virtio net device
 * @param ioport IOPort library instance to register virtio net ioport
 * @param iobase starting BAR port for front end emulation to start from
 * @param iosize size of starting BAR port for front end emulation
 * @param interrupt_pin PCI interrupt pin e.g. INTA = 1, INTB = 2 ,...
 * @param interrupt_line PCI interrupt line for virtio net IRQS
 * @param backend function pointers to backend implementation. Can be initialised by
 *  virtio_net_default_backend for default methods.
 * @param emulate_bar Emulate read and writes accesses to the PCI device Base Address Registers.
 * @return pointer to an initialised virtio_net_t, NULL if error.
 */
virtio_net_t *common_make_virtio_net(vm_t *vm, vmm_pci_space_t *pci, vmm_io_port_list_t *ioport,
                                     unsigned int iobase, size_t iobase_size, unsigned int interrupt_pin, unsigned int interrupt_line,
                                     struct raw_iface_funcs backend, bool emulate_bar_access);

/**
* @return a struct with a default virtio_net backend. It is the responsibility of the caller to
*  update these function pointers with its own custom backend.
*/
struct raw_iface_funcs virtio_net_default_backend(void);
