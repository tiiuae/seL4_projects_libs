/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef SEL4ARM_VMM_TK1_MAP_H
#define SEL4ARM_VMM_TK1_MAP_H

/***** Physical Map ****/
#define RAM_BASE  0x80000000
#define RAM_END   0xC0000000
#define RAM_SIZE (RAM_END - RAM_BASE)

/* a place holder */
#define ALIVE_PADDR 0x00000000

#endif /* SEL4ARM_VMM_TK1_MAP_H */
