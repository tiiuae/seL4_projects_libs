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

#include <sel4vm/guest_vm.h>

#include <autoconf.h>
#include <sel4vm/gen_config.h>
#include <sel4vm/devices.h>

#include <sel4vm/plat/devices.h>

/**
 * Check if a physical address should be handled by the provided device
 * @param[in] addr The physical address in question
 * @param[in] d    A description of the device to check
 * @return         non-zero if the address belongs to the device
 */
static inline int dev_paddr_in_range(uintptr_t addr, const struct device *d)
{
    return ((addr >= d->pstart) && addr < (d->pstart + d->size));
}
