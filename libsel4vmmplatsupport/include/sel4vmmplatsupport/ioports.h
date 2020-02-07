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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef int (*ioport_in_fn)(void *cookie, unsigned int port_no, unsigned int size, unsigned int *result);
typedef int (*ioport_out_fn)(void *cookie, unsigned int port_no, unsigned int size, unsigned int value);

typedef enum ioport_type {
    IOPORT_FREE,
    IOPORT_ADDR
} ioport_type_t;

typedef struct ioport_range {
    uint16_t start;
    uint16_t end;
    uint16_t size;
} ioport_range_t;

typedef struct ioport_interface {
    void *cookie;
    /* ioport handler functions */
    ioport_in_fn port_in;
    ioport_out_fn port_out;
    /* ioport description (for debugging) */
    const char *desc;
} ioport_interface_t;

typedef struct ioport_entry {
    ioport_range_t range;
    ioport_interface_t interface;
} ioport_entry_t;

typedef struct vmm_io_list {
    int num_ioports;
    /* Sorted list of ioport functions */
    ioport_entry_t **ioports;
    uint16_t alloc_addr;
} vmm_io_port_list_t;

/*
 * Initialize the io port list manager.
 * @param io_list Pointer to io_port list handle. This will be allocated and initialised
 * @param ioport_alloc_addr Base ioport address we can safely bump allocate from (doesn't conflict with other ioports).
 *                          This is used when registering ioport handlers of type 'IOPORT_FREE'
 * @return 0 if handler, otherwise -1 for error
 */
int vmm_io_port_init(vmm_io_port_list_t **io_list, uint16_t ioport_alloc_addr);

/* Add an io port range for emulation */
ioport_entry_t *vmm_io_port_add_handler(vmm_io_port_list_t *io_list, ioport_range_t ioport_range,
                            ioport_interface_t ioport_interface, ioport_type_t port_type);

/*
 * From a set of registered ioports, emulate an io instruction given a current ioport access.
 * @param io_port List of registered ioports with in/out handlers
 * @param port_no ioport address being accessed
 * @param is_in true if we are performing an io in operation, otherwise false
 * @param size size of io access
 * @param data pointer with the data being written if io-out op, otherwise will be populated with data from an io-in op
 * @return 0 if handled, 1 if unhandled, otherwise -1 for error
 */
int emulate_io_handler(vmm_io_port_list_t *io_port, unsigned int port_no, bool is_in, size_t size, unsigned int *data);
