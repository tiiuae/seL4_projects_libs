/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#ifndef _KEYBOARD_CODES_H_
#define _KEYBOARD_CODES_H_

/* Convert a keyboard scan code (from set 2) to its character representation.
 * Returns 0 for anything that is not a scan code for a canonical character.
 */
char sel4keyboard_code_to_char(int index);

#endif
