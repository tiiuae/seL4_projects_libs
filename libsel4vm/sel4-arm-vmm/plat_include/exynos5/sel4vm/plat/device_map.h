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

#include <autoconf.h>
#include <sel4vm/gen_config.h>

/***** Physical Map ****/
#define RAM_BASE  0x40000000
#define RAM_END   0xC0000000
#define RAM_SIZE (RAM_END - RAM_BASE)
