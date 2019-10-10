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

#include <sel4vm/guest_vm.h>
#include <sel4vm/ioports.h>
#include <sel4vmmplatsupport/drivers/pci_helper.h>

#include <pci/virtual_pci.h>
#include <pci/helper.h>

int vmm_pci_helper_map_bars(vm_t *vm, libpci_device_iocfg_t *cfg, vmm_pci_bar_t *bars);

/* Functions for emulating PCI config spaces over IO ports */
ioport_fault_result_t vmm_pci_io_port_in(void *cookie, unsigned int port_no, unsigned int size, unsigned int *result);
ioport_fault_result_t vmm_pci_io_port_out(void *cookie, unsigned int port_no, unsigned int size, unsigned int value);
