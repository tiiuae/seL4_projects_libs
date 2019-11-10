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

#include <sel4/sel4.h>

typedef enum processor_mode {
    PMODE_EL3h = 0b1101,
    PMODE_EL3t = 0b1100,
    PMODE_EL2h = 0b1001,
    PMODE_EL2t = 0b1000,
    PMODE_EL1h = 0b0101,
    PMODE_EL1t = 0b0100,
    PMODE_EL0t = 0b0000
} processor_mode_t;

#define seL4_UnknownSyscall_ARG0 seL4_UnknownSyscall_X0
