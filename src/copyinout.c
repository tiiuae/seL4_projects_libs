/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "vm.h"

#include <string.h>

#include <elf/elf.h>

#define PAGE_SIZE (1U << 12)

//#define DEBUG_COPYOUT
#ifdef DEBUG_COPYOUT
#define dprintf(...) printf("copyout: " __VA_ARGS__)
#else
#define dprintf(...) do{}while(0)
#endif


static int
copy_out_page(vspace_t *dst_vspace, vspace_t *src_vspace, vka_t* vka, void* src, void* dst, int size)
{
    vka_object_t frame;
    reservation_t res;
    void* tmp_dst;
    int err;

    assert(size <= PAGE_SIZE);
    dprintf("copy out page 0x%x->0x%x (0x%x bytes)\n", (uint32_t)src, (uint32_t)dst, size);

    /* Create a frame */
    err = vka_alloc_frame(vka, 12, &frame);
    assert(!err);
    if (err) {
        return -1;
    }

    /* Copy the data to the frame */
    tmp_dst = vspace_map_pages(src_vspace, &frame.cptr, NULL, seL4_AllRights,
                               1, 12, 1);
    assert(tmp_dst);
    printf("cap: 0x%x\n", frame.cptr);
    if (!tmp_dst) {
        return 1;
    }
    memcpy(tmp_dst + ((uintptr_t)dst & 0xfff), src, size);
    vspace_unmap_pages(src_vspace, tmp_dst, 1, 12, NULL);
    /* Map the frame to the dest vspace */
    res = vspace_reserve_range_at(dst_vspace, dst, 0x1000, seL4_AllRights, 1);
    if (!res.res) {
        assert(res.res);
        return -1;
    }
    err = vspace_map_pages_at_vaddr(dst_vspace, &frame.cptr, NULL, dst, 1, 12, res);
    vspace_free_reservation(dst_vspace, res);
    if (err) {
        printf("Failed to provide memory (%d)\n", err);
        vka_free_object(vka, &frame);
        assert(!err);
        return err;
    }
    /* Done */
    return err;
}

static int
copy_out(vspace_t *dst_vspace, vspace_t *src_vspace, vka_t* vka, void* src, uintptr_t dest, size_t size)
{
    dprintf("copy out 0x%x->0x%x (0%x bytes)\n", (uint32_t)src, (uint32_t)dest, size);
    while (size) {
        int seg_size;
        int err;
        seg_size = (size < PAGE_SIZE) ? size : PAGE_SIZE;
        err = copy_out_page(dst_vspace, src_vspace, vka, src, (void*)dest, seg_size);
        assert(!err);
        if (err) {
            return err;
        }
        dest += seg_size;
        src += seg_size;
        size -= seg_size;
    }
    return 0;
}


int
vm_copyout(vm_t* vm, void* data, uintptr_t address, size_t size)
{
    return copy_out(vm_get_vspace(vm), vm->vmm_vspace, vm->vka, data, address, size);
}


void*
vm_copyout_elf(vm_t* vm, void* elf_file)
{
    uint32_t entry;
    int num_headers;
    int i;

    entry = elf_getEntryPoint(elf_file);
    if (entry == 0) {
        return NULL;
    }

    num_headers = elf_getNumProgramHeaders(elf_file);
    for (i = 0; i < num_headers; i++) {
        if (elf_getProgramHeaderType(elf_file, i) == PT_LOAD) {
            unsigned long size, ipa;
            char *data;
            int err;

            data = elf_file + elf_getProgramHeaderOffset(elf_file, i);
            size = elf_getProgramHeaderFileSize(elf_file, i);
            ipa = elf_getProgramHeaderVaddr(elf_file, i);

            err = vm_copyout(vm, data, ipa, size);
            if (err) {
                return NULL;
            }
        }
    }
    return (void*)entry;
}

