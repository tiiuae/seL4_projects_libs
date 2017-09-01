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
#include <vspace/vspace.h>
#include <vka/vka.h>

int vmm_get_guest_vspace(vspace_t *loader, vspace_t *new_vspace, vka_t *vka, seL4_CPtr pd);

#ifdef CONFIG_ARM_SMMU
int vmm_guest_vspace_add_iospace(vspace_t *loader, vspace_t *vspace, seL4_CPtr iospace);
#endif
