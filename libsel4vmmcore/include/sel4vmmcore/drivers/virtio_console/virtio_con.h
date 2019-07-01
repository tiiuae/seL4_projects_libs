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

#include <sel4vmmcore/util/io.h>
#include <sel4pci/pci.h>
#include <sel4pci/virtio_emul.h>

/* Virtio console driver interface */
typedef struct virtio_con {
    /* IO Port base for virtio con device */
    unsigned int iobase;
    /* Virtio console emulation interface: VMM <-> Guest */
    virtio_emul_t *emul;
    /* Virtio console emulation functions: VMM <-> Guest */
    struct console_passthrough emul_driver_funcs;
    ps_io_ops_t ioops;
} virtio_con_t;

/**
 * Initialise a new virtio_con device with Base Address Registers (BARs) starting at iobase and backend functions
 * specified by the console_passthrough struct.
 * @param emul_vm arch specfic vm cookie
 * @param pci PCI library instance to register virtio con device
 * @param ioport IOPort library instance to register virtio con ioport
 * @param iobase starting BAR port for front end emulation to start from
 * @param iosize size of starting BAR port for front end emulation
 * @param interrupt_pin PCI interrupt pin e.g. INTA = 1, INTB = 2 ,...
 * @param interrupt_line PCI interrupt line for virtio con IRQS
 * @param backend function pointers to backend implementation. Can be initialised by
 *  virtio_con_default_backend for default methods.
 * @return pointer to an initialised virtio_con_t, NULL if error.
 */
virtio_con_t *common_make_virtio_con(virtio_emul_vm_t *emul_vm,
                                     vmm_pci_space_t *pci,
                                     vmm_io_port_list_t *ioport,
                                     unsigned int iobase,
                                     size_t iobase_size,
                                     unsigned int interrupt_pin,
                                     unsigned int interrupt_line,
                                     struct console_passthrough backend);
