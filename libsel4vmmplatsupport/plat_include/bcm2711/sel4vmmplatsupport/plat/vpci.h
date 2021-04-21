/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

/* PCI host bridge memory regions are defined in the pci dts node
 * supplied to the Linux guest. These values are also reflected here.
 */

// Mapped at 0xf0000000, size 0x3000000 (48MB)
//
// Note that the mapping address must be inside 32-bit address space,
// since on guest Linux the PCI resource does not have IORESOURCE_MEM_64
// set (see pci_bus_alloc_resource in drivers/pci/bus.c) and therefore
// it tries to use 32-bit address space only.  TODO: find out whether
// this is something that can be configured in the guest, or does the
// BAR registers and/or VMM's emulated PCI device have something to do
// with it.

/* PCI host bridge configration space */
#define PCI_CFG_REGION_ADDR 0xf0000000
/* PCI host bridge IO space */
#define PCI_IO_REGION_ADDR 0xf1000000
/* Size of PCI configuration space */
#define PCI_CFG_REGION_SIZE 0x1000000   // 16MB
/* Size of PCI IO space  */
#define PCI_IO_REGION_SIZE 0x10000      // 64kB
/* PCI memory space */
#define PCI_MEM_REGION_ADDR 0xf2000000ull
/* PCI memory space size */
#define PCI_MEM_REGION_SIZE 0x1000000   // 16MB

/* FDT IRQ controller address cells definition */
#define GIC_ADDRESS_CELLS 0x1
