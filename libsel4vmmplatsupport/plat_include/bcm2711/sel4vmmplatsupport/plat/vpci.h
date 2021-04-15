/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

/* PCI host bridge memory regions are defined in the pci dts node
 * supplied to the Linux guest. These values are also reflected here.
 */

// Mapped at 0x100000000, size 0x3000000 (48MB)

/* PCI host bridge configration space */
#define PCI_CFG_REGION_ADDR 0x100000000 //0x3E000000
/* PCI host bridge IO space */
#define PCI_IO_REGION_ADDR 0x101000000  // 0x3D000000
/* Size of PCI configuration space */
#define PCI_CFG_REGION_SIZE 0x1000000   // 16MB
/* Size of PCI IO space  */
#define PCI_IO_REGION_SIZE 0x10000      // 64kB
/* PCI memory space */
#define PCI_MEM_REGION_ADDR 0x102000000ull // 0x3F000000ull
/* PCI memory space size */
#define PCI_MEM_REGION_SIZE 0x1000000   // 16MB

/* FDT IRQ controller address cells definition */
#define GIC_ADDRESS_CELLS 0x2
