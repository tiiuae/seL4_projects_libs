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

#include <sel4arm-vmm/vm.h>

/* PCI host bridge memory regions are defined in the pci dts node
 * supplied to the Linux guest. These values are also reflected here */

/* PCI host bridge configration space */
#define PCI_CFG_REGION_ADDR 0x3E000000
/* PCI host bridge IO space */
#define PCI_IO_REGION_ADDR 0x3D000000
/* Size of PCI configuration space */
#define PCI_CFG_REGION_SIZE 0x1000000
/* Size of PCI IO space  */
#define PCI_CFG_IO_REGION_SIZE 0x1000

/* Mask to retrieve PCI bar size */
#define PCI_CFG_BAR_MASK 0xFFFFFFFF

int vm_install_vpci(vm_t *vm);

