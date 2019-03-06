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

#include <stdint.h>

typedef int (*ioport_in_fn)(void *cookie, unsigned int port_no, unsigned int size, unsigned int *result);
typedef int (*ioport_out_fn)(void *cookie, unsigned int port_no, unsigned int size, unsigned int value);

typedef struct ioport_range {
    unsigned int port_start;
    unsigned int port_end;

    int passthrough;

    /* If not passthrough, then handler functions */
    void *cookie;
    ioport_in_fn port_in;
    ioport_out_fn port_out;

    const char* desc;
} ioport_range_t;

typedef struct vmm_io_list {
    int num_ioports;
    /* Sorted list of ioport functions */
    ioport_range_t *ioports;
} vmm_io_port_list_t;

/* Initialize the io port list manager */
int vmm_io_port_init(vmm_io_port_list_t *io_list);

/* Add an io port range for emulation */
int vmm_io_port_add_handler(vmm_io_port_list_t *io_list, uint16_t start, uint16_t end, void *cookie, ioport_in_fn port_in, ioport_out_fn port_out, const char *desc);

int emulate_io_handler(vmm_io_port_list_t *io_port, unsigned int port_no, int is_in, int size, unsigned int *data);
