/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

// Deals with ranges of memory mapped io

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sel4/sel4.h>

#include <sel4vm/guest_vm.h>
#include "sel4vm/debug.h"
#include "sel4vm/vmm.h"
#include "sel4vm/platform/vmcs.h"
#include "sel4vm/platform/vmexit.h"
#include "sel4vm/mmio.h"
#include "sel4vm/processor/decode.h"

int vmm_mmio_init(vmm_mmio_list_t *list) {
    list->num_ranges = 0;
    list->ranges = malloc(0);
    assert(list->ranges);

    return 0;
}

static int range_cmp(const void *a, const void *b) {
    return ((const vmm_mmio_range_t *)a)->start - ((const vmm_mmio_range_t *)b)->start;
}

int vmm_mmio_add_handler(vmm_mmio_list_t *list, uintptr_t start, uintptr_t end,
        void *cookie, const char *name,
        vmm_mmio_read_fn read_handler, vmm_mmio_write_fn write_handler) {
    vmm_mmio_range_t *new = malloc(sizeof(*new));
    new->start = start;
    new->end = end;
    new->cookie = cookie;
    new->read_handler = read_handler;
    new->write_handler = write_handler;
    new->name = name;

    list->ranges = realloc(list->ranges, sizeof(vmm_mmio_range_t) * (list->num_ranges + 1));
    memcpy(&list->ranges[list->num_ranges++], new, sizeof(vmm_mmio_range_t));

    qsort(list->ranges, list->num_ranges, sizeof(vmm_mmio_range_t), range_cmp);

    return 0;
}
