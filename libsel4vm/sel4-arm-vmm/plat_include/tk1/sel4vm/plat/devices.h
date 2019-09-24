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
#include <sel4vm/plat/device_map.h>
#include <sel4vm/vm.h>

#define GIC_PADDR   0x50040000
#define MAX_VIRQS   200

extern const struct device dev_vram;
