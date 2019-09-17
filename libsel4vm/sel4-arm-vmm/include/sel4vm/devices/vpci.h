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

#include <sel4vm/vm.h>
#include <sel4vm/plat/vpci.h>

/* Mask to retrieve PCI bar size */
#define PCI_CFG_BAR_MASK 0xFFFFFFFF

int vm_install_vpci(vm_t *vm, vmm_io_port_list_t *io_port, vmm_pci_space_t *pci);
