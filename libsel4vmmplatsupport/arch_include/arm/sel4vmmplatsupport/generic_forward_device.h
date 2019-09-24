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
#include <sel4vm/guest_vm.h>

/**
 * This virtual device is used for dispatching faults to external handlers.
 * For example when using CAmkES, a device frame for a real device can be given
 * to a different CAmkES component and this virtual device will forward read and
 * write faults over a CAmkES interface so the component can perform or emulate
 * the actions.
 */

typedef void (*forward_write_fn)(uint32_t addr, uint32_t value);
typedef uint32_t (*forward_read_fn)(uint32_t addr);

struct generic_forward_cfg {
    forward_write_fn write_fn;
    forward_read_fn read_fn;
};

int vm_install_generic_forward_device(vm_t *vm, const struct device *d,
                                      struct generic_forward_cfg cfg);
