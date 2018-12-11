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

#include <sel4arm-vmm/sel4_arch/fault.h>
#include <assert.h>

seL4_Word *decode_rt(int reg, seL4_UserContext* c)
{
    switch (reg) {
    case  0:
        return &c->r0;
    case  1:
        return &c->r1;
    case  2:
        return &c->r2;
    case  3:
        return &c->r3;
    case  4:
        return &c->r4;
    case  5:
        return &c->r5;
    case  6:
        return &c->r6;
    case  7:
        return &c->r7;
    case  8:
        return &c->r8;
    case  9:
        return &c->r9;
    case 10:
        return &c->r10;
    case 11:
        return &c->r11;
    case 12:
        return &c->r12;
    case 13:
        return &c->sp;
    case 14:
        return &c->r14; /* lr */
    case 15:
        return &c->pc;
    default:
        assert(!"Invalid register");
        return NULL;
    }
};
