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

typedef ioport_fault_result_t (*unhandled_ioport_callback_fn)(vm_t *vm, unsigned int port_no, bool is_in, unsigned int *value,
        size_t size, void *cookie);
/* IOPort fault callback registration functions */
int vm_register_unhandled_ioport_callback(vm_t *vm, unhandled_ioport_callback_fn ioport_callback,
                                      void *cookie);

int vm_enable_passthrough_ioport(vm_vcpu_t *vcpu, uint16_t port_start, uint16_t port_end);
