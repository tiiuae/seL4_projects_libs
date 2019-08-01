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
#include <autoconf.h>
#include <sel4vm/gen_config.h>
#include <sel4utils/mapping.h>
#include "vm.h"
#include <stdlib.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_vm_util.h>

#include <sel4vm/devices.h>
#include <sel4vm/plat/devices.h>

#include <sel4vm/fault.h>
#include <vka/capops.h>

//#define DEBUG_MAPPINGS

#ifdef DEBUG_MAPPINGS
#define DMAP(...) printf(__VA_ARGS__)
#else
#define DMAP(...) do{}while(0)
#endif

int
vm_map_frame(vm_t *vm, seL4_CPtr cap, uintptr_t ipa, size_t size_bits, int cached, seL4_CapRights_t vm_rights)
{
    void *addr = (void *)ipa;
    reservation_t res;
    vspace_t *vm_vspace = vm_get_vspace(vm);
    int err;

    assert(vm_vspace != NULL);
    assert(addr != NULL);

    res = vspace_reserve_range_at(vm_vspace, addr, BIT(size_bits), vm_rights, cached);
    if (!res.res) {
        return -1;
    }
    err = vspace_map_pages_at_vaddr(vm_vspace, &cap, NULL, addr, 1, size_bits, res); //  NULL = cookies 1 = num caps
    vspace_free_reservation(vm_vspace, res);
    if (err) {
        printf("Failed to provide memory\n");
        return -1;
    }

    return 0;
}
