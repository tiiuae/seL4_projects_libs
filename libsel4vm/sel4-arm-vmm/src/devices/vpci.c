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

#include <sel4pci/pci_helper.h>
#include <sel4pci/pci.h>
#include <sel4vmmcore/util/io.h>
#include <sel4vm/devices.h>
#include <sel4vm/devices/vpci.h>

static int width_to_size(enum fault_width fw)
{

    if (fw == WIDTH_BYTE) {
        return 1;
    } else if (fw == WIDTH_HALFWORD) {
        return 2;
    } else if (fw == WIDTH_WORD) {
        return 4;
    } else if (fw == WIDTH_DOUBLEWORD) {
        return 8;
    }
    return 0;
}

static void pci_cfg_read_fault(struct device *d, vm_t *vm, fault_t *fault, vmm_pci_address_t pci_addr,
                               uint8_t offset, vmm_pci_entry_t *dev)
{
    seL4_Word data = 0;
    int err = 0;

    err = dev->ioread((void *)dev->cookie, offset, width_to_size(fault_get_width(fault)), &data);
    if (err) {
        ZF_LOGE("Failure performing read from PCI CFG device");
    }

    seL4_Word s = (fault_get_address(fault) & 0x3) * 8;
    fault_set_data(fault, data << s);
}

static void pci_cfg_write_fault(struct device *d, vm_t *vm, fault_t *fault, vmm_pci_address_t pci_addr,
                                uint8_t offset, vmm_pci_entry_t *dev)
{
    uint32_t mask;
    uint32_t value;
    uint8_t bar;
    int err;

    bar = (offset - PCI_BAR_OFFSET(0)) / sizeof(uint32_t);
    mask = fault_get_data_mask(fault);
    value = fault_get_data(fault) & mask;
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

static int pci_cfg_fault_handler(struct device *d, vm_t *vm, fault_t *fault)
{
    uint32_t addr;
    uint8_t offset;
    vmm_pci_address_t pci_addr;

    addr = fault_get_address(fault);
    addr -= PCI_CFG_REGION_ADDR;

    make_addr_reg_from_config(addr, &pci_addr, &offset);
    pci_addr.fun = 0;

    vmm_pci_entry_t *dev = find_device(vm->pci, pci_addr);
    if (!dev) {
        ZF_LOGW("Failed to find pci device B:%d D:%d F:%d", pci_addr.bus, pci_addr.dev, pci_addr.fun);
        /* No device found */
        return advance_fault(fault);
    }

    if (fault_is_read(fault)) {
        pci_cfg_read_fault(d, vm, fault, pci_addr, offset, dev);
    } else {
        pci_cfg_write_fault(d, vm, fault, pci_addr, offset, dev);
    }

    return advance_fault(fault);
}

static int pci_cfg_io_fault_handler(struct device *d, vm_t *vm, fault_t *fault)
{
    /* Get CFG Port address */
    uint16_t cfg_port = (fault_get_address(fault) - d->pstart) & USHRT_MAX;
    unsigned int value = 0;
    seL4_Word fault_data = 0;

    /* Determine io direction */
    bool is_in = false;
    if (fault_is_read(fault)) {
        is_in = true;
    } else {
        value = fault_get_data(fault);
    }
    /* Emulate IO */
    emulate_io_handler(vm->io_port, cfg_port, is_in, width_to_size(fault_get_width(fault)), (void *)&value);

    if (is_in) {
        memcpy(&fault_data, (void *)&value, width_to_size(fault_get_width(fault)));
        seL4_Word s = (fault_get_address(fault) & 0x3) * 8;
        fault_set_data(fault, fault_data << s);
    }

    return advance_fault(fault);
}

const struct device dev_vpci_cfg = {
    .devid = DEV_CUSTOM,
    .name = "vpci.cfg",
    .pstart = PCI_CFG_REGION_ADDR,
    .size = PCI_CFG_REGION_SIZE,
    .handle_page_fault = &pci_cfg_fault_handler,
    .priv = NULL,
};

const struct device dev_vpci_cfg_io = {
    .devid = DEV_CUSTOM,
    .name = "vpci.cfg_io",
    .pstart = PCI_IO_REGION_ADDR,
    .size = PCI_IO_REGION_SIZE,
    .handle_page_fault = &pci_cfg_io_fault_handler,
    .priv = NULL,
};

int vm_install_vpci(vm_t *vm)
{

    /* Install base VPCI CFG region */
    int err = vm_add_device(vm, &dev_vpci_cfg);
    if (err) {
        ZF_LOGE("Failed to install VPCI CFG region");
        return err;
    }

    /* Install base VPCI CFG IOPort region */
    err = vm_add_device(vm, &dev_vpci_cfg_io);
    if (err) {
        ZF_LOGE("Failed to install VPCI CFG IOPort region");
        return err;
    }

    /* Add VPCI CFG IOPort handlers */
    /* PCI_CONFIG_DATA */
    ioport_range_t config_data_range = {PCI_CONF_PORT_DATA, PCI_CONF_PORT_DATA_END};
    ioport_interface_t config_data_interface = {&vm->pci, vmm_pci_io_port_in, vmm_pci_io_port_out, "PCI_CONF_PORT_DATA"};
    err = vmm_io_port_add_handler(vm->io_port, config_data_range, config_data_interface);
    if (err) {
        ZF_LOGE("Failed to register IOPort handler for PCI_CONF_PORT_DATA (Port: 0x%x-0x%x)", PCI_CONF_PORT_DATA,
                PCI_CONF_PORT_DATA_END);
        return -1;
    }
    /* PCI_CONFIG_ADDRESS */
    ioport_range_t config_address_range = {PCI_CONF_PORT_DATA, PCI_CONF_PORT_ADDR_END};
    ioport_interface_t config_address_interface = {&vm->pci, vmm_pci_io_port_in, vmm_pci_io_port_out, "PCI_CONF_PORT_ADDR"};
    err = vmm_io_port_add_handler(vm->io_port, config_address_range, config_address_interface);
    if (err) {
        ZF_LOGE("Failed to register IOPort handler for PCI_CONF_PORT_ADDR (Port: 0x%x-0x%x)", PCI_CONF_PORT_ADDR,
                PCI_CONF_PORT_ADDR_END);
        return -1;
    }

}
