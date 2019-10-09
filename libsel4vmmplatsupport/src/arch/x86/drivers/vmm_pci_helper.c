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
#include <sel4vm/guest_memory_util.h>
#include <sel4vm/io.h>
#include <sel4vm/boot.h>

#include <sel4vmmplatsupport/drivers/vmm_pci_helper.h>

int vmm_pci_helper_map_bars(vm_t *vm, libpci_device_iocfg_t *cfg, vmm_pci_bar_t *bars)
{
    int i;
    int bar = 0;
    for (i = 0; i < 6; i++) {
        if (cfg->base_addr[i] == 0) {
            continue;
        }
        size_t size = cfg->base_addr_size[i];
        assert(size != 0);
        int size_bits = BYTES_TO_SIZE_BITS(size);
        if (BIT(size_bits) != size) {
            ZF_LOGE("PCI bar is not power of 2 size (%zu)", size);
            return -1;
        }
        bars[bar].size_bits = size_bits;
        if (cfg->base_addr_space[i] == PCI_BASE_ADDRESS_SPACE_MEMORY) {
            /* Need to map into the VMM. Make sure it is aligned */
            uintptr_t addr;
            vm_memory_reservation_t *reservation = vm_reserve_anon_memory(vm, size, default_error_fault_callback, NULL,
                    &addr);
            if (!reservation) {
                ZF_LOGE("Failed to reserve PCI bar %p size %zu", (void*)(uintptr_t)cfg->base_addr[i], size);
                return -1;
            }
            int err = map_ut_alloc_reservation_with_base_paddr(vm, (uintptr_t)cfg->base_addr[i], reservation);
            if (err) {
                ZF_LOGE("Failed to map PCI bar %p size %zu", (void*)(uintptr_t)cfg->base_addr[i], size);
                return -1;
            }
            bars[bar].address = addr;
            if (cfg->base_addr_prefetchable[i]) {
                bars[bar].mem_type = PREFETCH_MEM;
            } else {
                bars[bar].mem_type = NON_PREFETCH_MEM;
            }
        } else {
            /* Need to add the IO port range */
            int error = vm_enable_passthrough_ioport(vm->vcpus[BOOT_VCPU], cfg->base_addr[i], cfg->base_addr[i] + size - 1);
            if (error) {
                return error;
            }
            bars[bar].mem_type = NON_MEM;
            bars[bar].address = cfg->base_addr[i];
        }
        bar++;
    }
    return bar;
}
