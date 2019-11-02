/*
 * Copyright 2017, Data61
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
#include <sel4vm/guest_ram.h>
#include <sel4vm/plat/devices.h>
#include "../../../vm.h"

int
vm_install_ram_range(vm_t *vm, uintptr_t start, size_t size, bool untyped)
{
    return vm_ram_register_at(vm, start, size, untyped);
}
