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

#include <platsupport/io.h>
#include <ethdrivers/raw.h>
#include <sel4pci/vmm_virtio_emul.h>

struct ethif_virtio_emul_internal;

typedef struct ethif_virtio_emul {
    /* pointer to internal information */
    struct ethif_virtio_emul_internal *internal;
    /* io port interface functions */
    int (*io_in)(struct ethif_virtio_emul *emul, unsigned int offset, unsigned int size, unsigned int *result);
    int (*io_out)(struct ethif_virtio_emul *emul, unsigned int offset, unsigned int size, unsigned int value);
    /* notify of a status change in the underlying driver.
     * typically this would be due to link coming up
     * meaning that transmits can finally happen */
    int (*notify)(struct ethif_virtio_emul *emul);
} ethif_virtio_emul_t;

ethif_virtio_emul_t *ethif_virtio_emul_init(ps_io_ops_t io_ops, int queue_size, virtio_emul_vm_t *emul_vm, ethif_driver_init driver, void *config);

int vm_guest_write_mem(virtio_emul_vm_t *emul_vm, void *data, uintptr_t address, size_t size);
int vm_guest_read_mem(virtio_emul_vm_t *emul_vm, void *data, uintptr_t address, size_t size);
