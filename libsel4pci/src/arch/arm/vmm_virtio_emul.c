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
#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_vspace.h>

int vm_guest_write_mem(virtio_emul_vm_t *emul_vm, void *data, uintptr_t address, size_t size)
{
    return vm_guest_vspace_touch(&emul_vm->vm->mem.vm_vspace, address, size, vm_guest_set_phys_data_help, data);
}

int vm_guest_read_mem(virtio_emul_vm_t *emul_vm, void *data, uintptr_t address, size_t size)
{
    return vm_guest_vspace_touch(&emul_vm->vm->mem.vm_vspace, address, size, vm_guest_get_phys_data_help, data);
}
