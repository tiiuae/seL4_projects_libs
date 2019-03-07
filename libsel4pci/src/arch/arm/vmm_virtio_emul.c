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

int vm_guest_write_mem(virtio_emul_vm_t *emul_vm, void *data, uintptr_t address, size_t size) {
    return vm_copyout(emul_vm->vm, data, address, size);
}

int vm_guest_read_mem(virtio_emul_vm_t *emul_vm, void *data, uintptr_t address, size_t size) {
    return vm_copyin(emul_vm->vm, data, address, size);
}
