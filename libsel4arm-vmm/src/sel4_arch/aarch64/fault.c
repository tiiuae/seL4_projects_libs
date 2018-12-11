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
#include <sel4arm-vmm/fault.h>
#include <assert.h>

static seL4_Word wzr = 0;
seL4_Word *decode_rt(int reg, seL4_UserContext* c)
{
    switch (reg) {
    case  0:
        return &c->x0;
    case  1:
        return &c->x1;
    case  2:
        return &c->x2;
    case  3:
        return &c->x3;
    case  4:
        return &c->x4;
    case  5:
        return &c->x5;
    case  6:
        return &c->x6;
    case  7:
        return &c->x7;
    case  8:
        return &c->x8;
    case  9:
        return &c->x9;
    case 10:
        return &c->x10;
    case 11:
        return &c->x11;
    case 12:
        return &c->x12;
    case 13:
        return &c->x13;
    case 14:
        return &c->x14;
    case 15:
        return &c->x15;
    case 16:
        return &c->x16;
    case 17:
        return &c->x17;
    case 18:
        return &c->x18;
    case 19:
        return &c->x19;
    case 20:
        return &c->x20;
    case 21:
        return &c->x21;
    case 22:
        return &c->x22;
    case 23:
        return &c->x23;
    case 24:
        return &c->x24;
    case 25:
        return &c->x25;
    case 26:
        return &c->x26;
    case 27:
        return &c->x27;
    case 28:
        return &c->x28;
    case 29:
        return &c->x29;
    case 30:
        return &c->x30;
    case 31:
        return &wzr;
    default:
        printf("invalid reg %d\n", reg);
        assert(!"Invalid register");
        return NULL;
    }
};
