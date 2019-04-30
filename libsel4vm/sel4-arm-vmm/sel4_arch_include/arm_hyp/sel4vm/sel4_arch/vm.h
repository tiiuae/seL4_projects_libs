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

#include <sel4/sel4.h>

#define MODE_USER       0x10
#define MODE_FIQ        0x11
#define MODE_IRQ        0x12
#define MODE_SUPERVISOR 0x13
#define MODE_MONITOR    0x16
#define MODE_ABORT      0x17
#define MODE_HYP        0x1a
#define MODE_UNDEFINED  0x1b
#define MODE_SYSTEM     0x1f

typedef enum processor_mode {
    PMODE_USER       = MODE_USER,
    PMODE_FIQ        = MODE_FIQ,
    PMODE_IRQ        = MODE_IRQ,
    PMODE_SUPERVISOR = MODE_SUPERVISOR,
    PMODE_MONITOR    = MODE_MONITOR,
    PMODE_ABORT      = MODE_ABORT,
    PMODE_HYPERVISOR = MODE_HYP,
    PMODE_UNDEFINED  = MODE_UNDEFINED,
    PMODE_SYSTEM     = MODE_SYSTEM
} processor_mode_t;

#define seL4_UnknownSyscall_ARG0 seL4_UnknownSyscall_R0

static inline void sel4arch_set_bootargs(seL4_UserContext *regs, seL4_Word pc,
                                         seL4_Word mach_type, seL4_Word atags)
{
    regs->r0 = 0;
    regs->r1 = mach_type;
    regs->r2 = atags;
    regs->pc = pc;
    regs->cpsr = MODE_SUPERVISOR;
}
