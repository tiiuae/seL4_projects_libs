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

#include <sel4pci/virtio_emul.h>
#include <sel4pci/vmm_virtio_emul.h>

static int read_guest_mem(uintptr_t phys, void *vaddr, size_t size, size_t offset, void *cookie) {
    memcpy(cookie + offset, vaddr, size);
    return 0;
}

static int write_guest_mem(uintptr_t phys, void *vaddr, size_t size, size_t offset, void *cookie) {
    memcpy(vaddr, cookie + offset, size);
    return 0;
}

int vm_guest_write_mem(virtio_emul_vm_t *emul_vm, void *data, uintptr_t address, size_t size) {
    return vmm_guest_vspace_touch(emul_vm->vmm->guest_vspace, address, size, write_guest_mem, data);
}

int vm_guest_read_mem(virtio_emul_vm_t *emul_vm, void *data, uintptr_t address, size_t size) {
    return vmm_guest_vspace_touch(emul_vm->vmm->guest_vspace, address, size, read_guest_mem, data);
}
