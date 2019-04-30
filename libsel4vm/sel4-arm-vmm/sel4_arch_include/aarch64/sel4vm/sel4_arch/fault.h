/*
 * Copyright 2018, Data61
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

#include <stdbool.h>

#include <utils/util.h>
#include <sel4vm/fault.h>

#define SRT_MASK    0x1f

static inline bool sel4arch_fault_is_thumb(fault_t *f)
{
    return CPSR_IS_THUMB(fault_get_ctx(f)->spsr);
}
