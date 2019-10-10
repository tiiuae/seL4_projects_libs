/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#pragma once

#include <stdint.h>

#include <sel4/sel4.h>
#include <simple/simple.h>

typedef struct vm vm_t;
typedef struct vm_vcpu vm_vcpu_t;

typedef enum ioport_fault_result {
    IO_FAULT_HANDLED,
    IO_FAULT_UNHANDLED,
    IO_FAULT_ERROR
} ioport_fault_result_t;

typedef ioport_fault_result_t (*vm_ioport_in_fn)(void *cookie, unsigned int port_no, unsigned int size, unsigned int *result);
typedef ioport_fault_result_t (*vm_ioport_out_fn)(void *cookie, unsigned int port_no, unsigned int size, unsigned int value);
typedef ioport_fault_result_t (*unhandled_ioport_callback_fn)(vm_t *vm, unsigned int port_no, bool is_in, unsigned int *value,
        size_t size, void *cookie);

typedef struct vm_ioport_range {
    uint16_t start;
    uint16_t end;
} vm_ioport_range_t;

typedef struct vm_ioport_interface {
    void *cookie;
    /* ioport handler functions */
    vm_ioport_in_fn port_in;
    vm_ioport_out_fn port_out;
    /* ioport description (for debugging) */
    const char *desc;
} vm_ioport_interface_t;

typedef struct vm_ioport_entry {
    vm_ioport_range_t range;
    vm_ioport_interface_t interface;
} vm_ioport_entry_t;

typedef struct vm_io_list {
    int num_ioports;
    /* Sorted list of ioport functions */
    vm_ioport_entry_t *ioports;
} vm_io_port_list_t;

/* Add an io port range for emulation */
int vm_io_port_add_handler(vm_t *vm, vm_ioport_range_t ioport_range,
                            vm_ioport_interface_t ioport_interface);

/* IOPort fault callback registration functions */
int vm_register_unhandled_ioport_callback(vm_t *vm, unhandled_ioport_callback_fn ioport_callback,
                                      void *cookie);

int vm_enable_passthrough_ioport(vm_vcpu_t *vcpu, uint16_t port_start, uint16_t port_end);
