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

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_memory.h>

int map_vm_memory_reservation(vm_t *vm, vm_memory_reservation_t *vm_reservation,
        memory_map_iterator_fn map_iterator, void *map_cookie);
