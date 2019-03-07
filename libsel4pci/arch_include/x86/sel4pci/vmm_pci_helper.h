/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#include <vmm/vmm.h>
#include <pci/virtual_pci.h>
#include <pci/helper.h>

int vmm_pci_helper_map_bars(vmm_t *vmm, libpci_device_iocfg_t *cfg, vmm_pci_bar_t *bars);
