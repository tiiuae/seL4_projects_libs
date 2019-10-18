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

int
vm_copyout_atags(vm_t* vm, struct atag_list* atags, uint32_t addr)
{
    vspace_t *vm_vspace, *vmm_vspace;
    void *vm_addr, *vmm_addr, *buf;
    reservation_t res;
    vka_t *vka;
    vka_object_t frame;
    size_t size;
    struct atag_list *atag_cur;
    int err;

    vka = vm->vka;
    vm_addr = (void *)(addr & ~0xffflu);
    vm_vspace = vm_get_vspace(vm);
    vmm_vspace = vm_get_vmm_vspace(vm);

    /* Make sure we don't cross a page boundary
     * NOTE: the next page will usually be used by linux for PT!
     */
    for (size = 0, atag_cur = atags; atag_cur != NULL; atag_cur = atag_cur->next) {
        size += atags_size_bytes(atag_cur);
    }
    size += 8; /* NULL tag */
    assert((addr & 0xfff) + size < 0x1000);

    /* Create a frame (and a copy for the VMM) */
    err = vka_alloc_frame(vka, 12, &frame);
    assert(!err);
    if (err) {
        return -1;
    }
    /* Map the frame to the VMM */
    vmm_addr = vspace_map_pages(vmm_vspace, &frame.cptr, NULL, seL4_AllRights, 1, 12, 0);
    assert(vmm_addr);

    /* Copy in the atags */
    buf = vmm_addr + (addr & 0xfff);
    for (atag_cur = atags; atag_cur != NULL; atag_cur = atag_cur->next) {
        int tag_size = atags_size_bytes(atag_cur);
        DVM("ATAG copy 0x%x<-0x%x %d\n", (uint32_t)buf, (uint32_t)atag_cur->hdr, tag_size);
        memcpy(buf, atag_cur->hdr, tag_size);
        buf += tag_size;
    }
    /* NULL tag terminator */
    memset(buf, 0, 8);

    /* Unmap the page and map it into the VM */
    vspace_unmap_pages(vmm_vspace, vmm_addr, 1, 12, NULL);
    res = vspace_reserve_range_at(vm_vspace, vm_addr, 0x1000, seL4_AllRights, 0);
    assert(res.res);
    if (!res.res) {
        vka_free_object(vka, &frame);
        return -1;
    }
    err = vspace_map_pages_at_vaddr(vm_vspace, &frame.cptr, NULL, vm_addr, 1, 12, res);
    vspace_free_reservation(vm_vspace, res);
    assert(!err);
    if (err) {
        printf("Failed to provide memory\n");
        vka_free_object(vka, &frame);
        return -1;
    }

    return 0;
}

int
vm_install_service(vm_t* vm, seL4_CPtr service, int index, uint32_t b)
{
    cspacepath_t src, dst;
    seL4_Word badge = b;
    int err;
    vka_cspace_make_path(vm->vka, service, &src);
    dst.root = vm->tcb.cspace.cptr;
    dst.capPtr = index;
    dst.capDepth = VM_CSPACE_SIZE_BITS;
    err =  vka_cnode_mint(&dst, &src, seL4_AllRights, badge);
    return err;
}

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

int vm_register_reboot_callback(vm_t *vm, reboot_hook_fn hook, void *token)
{
    if (hook == NULL) {
        ZF_LOGE("hook is NULL");
        return -1;
    }
    struct reboot_hooks rb = {
        .fn = hook,
        .token = token
    };
    if (vm->arch.nhooks < MAX_REBOOT_HOOKS_PER_VM) {
        vm->arch.rb_hooks[vm->arch.nhooks++] = rb;
        return 0;
    } else {
        return -1;
    }

}

int vm_process_reboot_callbacks(vm_t *vm) {
    for (int i = 0; i < vm->arch.nhooks; i++) {
        struct reboot_hooks rb = vm->arch.rb_hooks[i];
        if (rb.fn == NULL) {
            ZF_LOGE("NULL call back has been registered");
            return -1;
        }
        int err = rb.fn(vm, rb.token);
        if (err) {
            ZF_LOGE("Callback returned error: %d", err);
            return err;
        }
    }
    return 0;
}
