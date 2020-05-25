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

#define GIC_VERSION_2

#define GIC_PADDR   0x50040000

#define GIC_DIST_PADDR       (GIC_PADDR + 0x1000)
#define GIC_DIST_SIZE        0x1000

#define GIC_CPU_PADDR        (GIC_PADDR + 0x2000)
#define GIC_CPU_SIZE         0x1000

#define GIC_VCPU_CNTR_PADDR  (GIC_PADDR + 0x4000)
#define GIC_VCPU_CNTR_SIZE   0x1000

#define GIC_VCPU_PADDR       (GIC_PADDR + 0x6000)
#define GIC_VCPU_SIZE        0x1000
