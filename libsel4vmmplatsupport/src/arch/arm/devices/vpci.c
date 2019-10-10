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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sel4/sel4.h>

#include <pci/virtual_pci.h>
#include <pci/helper.h>
#include <pci/pci.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_vcpu_fault.h>

#include <sel4vmmplatsupport/drivers/pci_helper.h>
#include <sel4vmmplatsupport/drivers/pci.h>
#include <sel4vmmplatsupport/ioports.h>

#include <sel4vmmplatsupport/device.h>
#include <sel4vmmplatsupport/vpci.h>

struct pci_cfg_data {
    vmm_io_port_list_t *io_port;
    vmm_pci_space_t *pci;
};

static void pci_cfg_read_fault(struct device* d, vm_t *vm, vm_vcpu_t* vcpu, vmm_pci_address_t pci_addr,
                               uint8_t offset, vmm_pci_entry_t *dev)
{
    seL4_Word data = 0;
    int err = 0;

    err = dev->ioread((void *)dev->cookie, offset, get_vcpu_fault_size(vcpu), &data);
    if (err) {
        ZF_LOGE("Failure performing read from PCI CFG device");
    }

    seL4_Word s = (get_vcpu_fault_address(vcpu) & 0x3) * 8;
    set_vcpu_fault_data(vcpu, data << s);
}

static void pci_cfg_write_fault(struct device* d, vm_t *vm, vm_vcpu_t* vcpu, vmm_pci_address_t pci_addr,
                                uint8_t offset, vmm_pci_entry_t *dev)
{
    uint32_t mask;
    uint32_t value;
    uint8_t bar;
    int err;

    bar = (offset - PCI_BAR_OFFSET(0)) / sizeof(uint32_t);
    mask = get_vcpu_fault_data_mask(vcpu);
    value = get_vcpu_fault_data(vcpu) & mask;
    pci_bar_emulation_t *bar_emul = dev->cookie;
    /* Linux will mask the PCI bar expecting its next read to be the size of the bar.
    * To handle this we write the bars size to pci header such that the kernels next read will
    * be the size. */
    if (bar < 6 && value == PCI_CFG_BAR_MASK) {
        uint32_t bar_size =  BIT((bar_emul->bars[bar].size_bits));
        err = dev->iowrite((void *)dev->cookie, offset, sizeof(bar_size), bar_size);
    } else {
        err = dev->iowrite((void *)dev->cookie, offset, sizeof(value), value);
    }
    if (err) {
        ZF_LOGE("Failure writing to PCI CFG device");
    }
}

static memory_fault_result_t
pci_cfg_fault_handler(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr, size_t fault_length, void *cookie)
{
    uint8_t offset;
    vmm_pci_address_t pci_addr;
    struct device *dev = (struct device *)cookie;
    struct pci_cfg_data *cfg_data = (struct pci_cfg_data *)dev->priv;
    vmm_pci_space_t *pci = cfg_data->pci;

    fault_addr -= PCI_CFG_REGION_ADDR;

    make_addr_reg_from_config(fault_addr, &pci_addr, &offset);
    pci_addr.fun = 0;

    vmm_pci_entry_t *pci_dev = find_device(pci, pci_addr);
    if (!pci_dev) {
        ZF_LOGW("Failed to find pci device B:%d D:%d F:%d", pci_addr.bus, pci_addr.dev, pci_addr.fun);
        /* No device found */
        advance_vcpu_fault(vcpu);
        return FAULT_HANDLED;
    }

    if (is_vcpu_read_fault(vcpu)) {
        pci_cfg_read_fault(dev, vm, vcpu, pci_addr, offset, pci_dev);
    } else {
        pci_cfg_write_fault(dev, vm, vcpu, pci_addr, offset, pci_dev);
    }

    advance_vcpu_fault(vcpu);
    return FAULT_HANDLED;
}

static memory_fault_result_t
pci_cfg_io_fault_handler(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr, size_t fault_length, void *cookie)
{
    struct device *dev = (struct device *)cookie;
    /* Get CFG Port address */
    uint16_t cfg_port = (fault_addr - dev->pstart) & USHRT_MAX;
    unsigned int value = 0;
    seL4_Word fault_data = 0;
    struct pci_cfg_data *cfg_data = (struct pci_cfg_data *)dev->priv;
    vmm_io_port_list_t *io_port = cfg_data->io_port;

    /* Determine io direction */
    bool is_in = false;
    if (is_vcpu_read_fault(vcpu)) {
        is_in = true;
    } else {
        value = get_vcpu_fault_data(vcpu);
    }
    /* Emulate IO */
    emulate_io_handler(io_port, cfg_port, is_in, fault_length, (void *)&value);

    if (is_in) {
        memcpy(&fault_data, (void *)&value, fault_length);
        seL4_Word s = (fault_addr & 0x3) * 8;
        set_vcpu_fault_data(vcpu, fault_data << s);
    }

    advance_vcpu_fault(vcpu);
    return FAULT_HANDLED;
}

struct device dev_vpci_cfg = {
    .name = "vpci.cfg",
    .pstart = PCI_CFG_REGION_ADDR,
    .size = PCI_CFG_REGION_SIZE,
    .priv = NULL,
};

struct device dev_vpci_cfg_io = {
    .name = "vpci.cfg_io",
    .pstart = PCI_IO_REGION_ADDR,
    .size = PCI_IO_REGION_SIZE,
    .priv = NULL,
};

int vm_install_vpci(vm_t *vm, vmm_io_port_list_t *io_port, vmm_pci_space_t *pci)
{

    ps_io_ops_t *ops = vm->io_ops;
    struct pci_cfg_data *cfg_data;
    int err = ps_calloc(&ops->malloc_ops, 1, sizeof(struct pci_cfg_data), (void **)&cfg_data);
    if (err) {
        ZF_LOGE("Failed to install VPCI: Failed allocate pci cfg io data");
        return -1;
    }
    cfg_data->io_port = io_port;
    cfg_data->pci = pci;

    /* Install base VPCI CFG region */
    dev_vpci_cfg.priv = (void *)cfg_data;
    vm_memory_reservation_t *cfg_reservation = vm_reserve_memory_at(vm, dev_vpci_cfg.pstart, dev_vpci_cfg.size,
            pci_cfg_fault_handler, (void *)&dev_vpci_cfg);
    if (!cfg_reservation) {
        return -1;
    }

    /* Install base VPCI CFG IOPort region */
    dev_vpci_cfg_io.priv = (void *)cfg_data;
    vm_memory_reservation_t *cfg_io_reservation = vm_reserve_memory_at(vm, dev_vpci_cfg_io.pstart, dev_vpci_cfg_io.size,
            pci_cfg_io_fault_handler, (void *)&dev_vpci_cfg_io);
    if (!cfg_io_reservation) {
        return -1;
    }
    return 0;
}
