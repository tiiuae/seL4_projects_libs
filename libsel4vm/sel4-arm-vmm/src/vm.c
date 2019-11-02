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
#include <autoconf.h>
#include <sel4vm/gen_config.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/boot.h>
#include <sel4vm/guest_vm_util.h>
#include <sel4vm/guest_memory.h>

#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

#include <cpio/cpio.h>
#include <vka/object.h>
#include <vka/capops.h>
#include <string.h>
#include <sel4utils/mapping.h>
#include <utils/ansi.h>

#include <sel4/sel4.h>
#include <sel4/messages.h>

#include "devices.h"

#include <sel4vm/sel4_arch/vm.h>

//#define DEBUG_VM
//#define DEBUG_RAM_FAULTS
//#define DEBUG_DEV_FAULTS
//#define DEBUG_STRACE

#define VM_CSPACE_SIZE_BITS    4
#define VM_FAULT_EP_SLOT       1
#define VM_CSPACE_SLOT         2

#ifdef DEBUG_RAM_FAULTS
#define DRAMFAULT(...) printf(__VA_ARGS__)
#else
#define DRAMFAULT(...) do{}while(0)
#endif

#ifdef DEBUG_DEV_FAULTS
#define DDEVFAULT(...) printf(__VA_ARGS__)
#else
#define DDEVFAULT(...) do{}while(0)
#endif

#ifdef DEBUG_STRACE
#define DSTRACE(...) printf(__VA_ARGS__)
#else
#define DSTRACE(...) do{}while(0)
#endif

#ifdef DEBUG_VM
#define DVM(...) do{ printf(__VA_ARGS__);}while(0)
#else
#define DVM(...) do{}while(0)
#endif

extern char _cpio_archive[];

uintptr_t vm_ipa_to_pa(vm_t *vm, uintptr_t ipa_base, size_t size)
{
    seL4_ARM_Page_GetAddress_t ret;
    uintptr_t pa_base = 0;
    uintptr_t ipa;
    vspace_t *vspace;
    vspace = vm_get_vspace(vm);
    ipa = ipa_base;
    do {
        seL4_CPtr cap;
        int bits;
        /* Find the cap */
        cap = vspace_get_cap(vspace, (void *)ipa);
        if (cap == seL4_CapNull) {
            return 0;
        }
        /* Find mapping size */
        bits = vspace_get_cookie(vspace, (void *)ipa);
        assert(bits == 12 || bits == 21);
        /* Find the physical address */
        ret = seL4_ARM_Page_GetAddress(cap);
        if (ret.error) {
            return 0;
        }
        if (ipa == ipa_base) {
            /* Record the result */
            pa_base = ret.paddr + (ipa & MASK(bits));
            /* From here on, ipa and ret.paddr will be aligned */
            ipa &= ~MASK(bits);
        } else {
            /* Check for a contiguous mapping */
            if (ret.paddr - pa_base != ipa - ipa_base) {
                return 0;
            }
        }
        ipa += BIT(bits);
    } while (ipa - ipa_base < size);
    return pa_base;
}
