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

#include <sel4pci/pci_vm.h>

int vmm_pci_helper_map_bars(vmm_t *vmm, libpci_device_iocfg_t *cfg, vmm_pci_bar_t *bars) {
    int i;
    int bar = 0;
    for (i = 0; i < 6; i++) {
        if (cfg->base_addr[i] == 0) {
            continue;
        }
        size_t size = cfg->base_addr_size[i];
        assert(size != 0);
        int size_bits = 31 - CLZ(size);
        if (BIT(size_bits) != size) {
            ZF_LOGE("PCI bar is not power of 2 size (%zu)", size);
            return -1;
        }
        bars[bar].size_bits = size_bits;
        if (cfg->base_addr_space[i] == PCI_BASE_ADDRESS_SPACE_MEMORY) {
            /* Need to map into the VMM. Make sure it is aligned */
            uintptr_t addr = vmm_map_guest_device(vmm, cfg->base_addr[i], size, BIT(size_bits));
            if(addr == 0) {
                ZF_LOGE("Failed to map PCI bar %p size %zu", (void*)(uintptr_t)cfg->base_addr[i], size);
                return -1;
            }
            bars[bar].ismem = 1;
            bars[bar].address = addr;
            bars[bar].prefetchable = cfg->base_addr_prefetchable[i];
        } else {
            /* Need to add the IO port range */
            int error = vmm_io_port_add_passthrough(&vmm->io_port, cfg->base_addr[i], cfg->base_addr[i] + size - 1, "PCI Passthrough Device");
            if (error) {
                return error;
            }
            bars[bar].ismem=0;
            bars[bar].address = cfg->base_addr[i];
            bars[bar].prefetchable = 0;
        }
        bar++;
    }
    return bar;
}
