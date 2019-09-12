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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <platsupport/io.h>

#include <sel4vmmcore/drivers/virtio.h>
#include <sel4vmmcore/drivers/virtio_console/virtio_con.h>

#include <pci/helper.h>
#include <sel4pci/pci_helper.h>

#define QUEUE_SIZE 128

static ps_io_ops_t ops;

static int virtio_con_io_in(void *cookie, unsigned int port_no, unsigned int size, unsigned int *result)
{
    virtio_con_t *con = (virtio_con_t *)cookie;
    unsigned int offset = port_no - con->iobase;
    unsigned int val;
    int err = con->emul->io_in(con->emul, offset, size, &val);
    if (err) {
        return err;
    }
    *result = val;
    return 0;
}

static int virtio_con_io_out(void *cookie, unsigned int port_no, unsigned int size, unsigned int value)
{
    int ret;
    virtio_con_t *con = (virtio_con_t *)cookie;
    unsigned int offset = port_no - con->iobase;
    ret = con->emul->io_out(con->emul, offset, size, value);
    return 0;
}

static int emul_con_driver_init(struct console_passthrough *driver, ps_io_ops_t io_ops, void *config)
{
    virtio_con_t *con = (virtio_con_t *)config;
    *driver = con->emul_driver_funcs;
    return 0;
}

static vmm_pci_entry_t vmm_virtio_console_pci_bar(unsigned int iobase,
                                                  size_t iobase_size_bits, unsigned int interrupt_pin, unsigned int interrupt_line)
{
    vmm_pci_device_def_t *pci_config;
    int err = ps_calloc(&ops.malloc_ops, 1, sizeof(*pci_config), (void **)&pci_config);
    ZF_LOGF_IF(err, "Failed to allocate pci config");
    *pci_config = (vmm_pci_device_def_t) {
        .vendor_id = VIRTIO_PCI_VENDOR_ID,
        .device_id = VIRTIO_CONSOLE_PCI_DEVICE_ID,
        .command = PCI_COMMAND_IO | PCI_COMMAND_MEMORY,
        .header_type = PCI_HEADER_TYPE_NORMAL,
        .subsystem_vendor_id    = VIRTIO_PCI_SUBSYSTEM_VENDOR_ID,
        .subsystem_id       = VIRTIO_ID_CONSOLE,
        .interrupt_pin = interrupt_pin,
        .interrupt_line = interrupt_line,
        .bar0 = iobase | PCI_BASE_ADDRESS_SPACE_IO,
        .cache_line_size = 64,
        .latency_timer = 64,
        .prog_if = VIRTIO_PCI_CLASS_CONSOLE & 0xff,
        .subclass = (VIRTIO_PCI_CLASS_CONSOLE >> 8) & 0xff,
        .class_code = (VIRTIO_PCI_CLASS_CONSOLE >> 16) & 0xff,
    };
    vmm_pci_entry_t entry = (vmm_pci_entry_t) {
        .cookie = pci_config,
        .ioread = vmm_pci_mem_device_read,
        .iowrite = vmm_pci_mem_device_write
    };

    vmm_pci_bar_t bars[1] = {{
            .mem_type = NON_MEM,
            .address = iobase,
            .size_bits = iobase_size_bits
        }
    };
    return vmm_pci_create_passthrough_bar_emulation(entry, 1, bars);
}


virtio_con_t *common_make_virtio_con(vm_t *vm, vmm_pci_space_t *pci, vmm_io_port_list_t *ioport,
                                     unsigned int iobase, size_t iobase_size, unsigned int interrupt_pin, unsigned int interrupt_line,
                                     struct console_passthrough backend)
{
    size_t iobase_size_bits = BYTES_TO_SIZE_BITS(iobase_size);
    int err = ps_new_stdlib_malloc_ops(&ops.malloc_ops);
    ZF_LOGF_IF(err, "Failed to get malloc ops");

    vmm_pci_entry_t con_entry = vmm_virtio_console_pci_bar(iobase, iobase_size_bits, interrupt_pin,
                                                           interrupt_line);
    vmm_pci_add_entry(pci, con_entry, NULL);

    virtio_con_t *con;
    err = ps_calloc(&ops.malloc_ops, 1, sizeof(*con), (void **)&con);
    ZF_LOGF_IF(err, "Failed to allocate virtio con");

    con->iobase = iobase;

    ioport_range_t virtio_io_range = {iobase, iobase + iobase_size};
    ioport_interface_t virtio_io_interface = {con, virtio_con_io_in, virtio_con_io_out, "VIRTIO CON"};
    vmm_io_port_add_handler(ioport, virtio_io_range, virtio_io_interface);

    ps_io_ops_t ioops;
    con->emul_driver_funcs = backend;
    con->emul = virtio_emul_init(ioops, QUEUE_SIZE, vm, emul_con_driver_init, con, VIRTIO_CONSOLE);

    assert(con->emul);
    return con;
}
