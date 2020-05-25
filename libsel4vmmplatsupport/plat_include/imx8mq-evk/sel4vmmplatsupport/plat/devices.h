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

#define GIC_VERSION_3

#define GIC_PADDR               0x38800000

#define GIC_DIST_PADDR          (GIC_PADDR + 0x00000000)
#define GIC_DIST_SIZE           0x10000

#define GIC_REDIST_PADDR        (GIC_PADDR + 0x00080000)
#define GIC_REDIST_SIZE         0x10000

#define GIC_REDIST_SGI_PADDR    (GIC_REDIST_PADDR + 0x00010000)
#define GIC_REDIST_SGI_SIZE     0x10000
