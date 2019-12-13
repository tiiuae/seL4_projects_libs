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

#include <sel4utils/process.h>
#include <platsupport/io.h>

#include <elf/elf.h>
#include <simple/simple.h>
#include <vspace/vspace.h>
#include <vka/vka.h>

#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
#include <sel4vm/vchan_vm_component.h>
#include <sel4vchan/vchan_component.h>
#endif //CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT

typedef struct vm vm_t;
