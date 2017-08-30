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
#include <sel4arm-vmm/images.h>
#include <string.h>
#include <elf/elf.h>

#define UIMAGE_MAGIC 0x56190527
#define ZIMAGE_MAGIC 0x016F2818
#define DTB_MAGIC    0xedfe0dd0

struct dtb_hdr {
    uint32_t magic;
#if 0
    /* TODO check */
    uint32_t size;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t comp_version;
    uint32_t boot_cpuid;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
#endif
};

struct zimage_hdr {
    uint32_t code[9];
    uint32_t magic;
    uint32_t start;
    uint32_t end;
};


static int
is_uImage(void* file)
{
    uint32_t magic = UIMAGE_MAGIC;
    return memcmp(file, &magic, sizeof(magic));
}

static int
is_zImage(void* file)
{
    struct zimage_hdr* hdr;
    hdr = (struct zimage_hdr*)file;
    return hdr->magic != ZIMAGE_MAGIC;
}

static int
is_dtb(void* file)
{
    struct dtb_hdr* hdr;
    hdr = (struct dtb_hdr*)file;
    return hdr->magic != DTB_MAGIC;
}

enum img_type
image_get_type(void* file)
{
    if (elf_checkFile(file) == 0) {
        return IMG_ELF;
    } else if (is_zImage(file) == 0) {
        return IMG_ZIMAGE;
    } else if (is_uImage(file) == 0) {
        return IMG_UIMAGE;
    } else if (is_dtb(file) == 0) {
        return IMG_DTB;
    } else {
        return IMG_BIN;
    }
}

uintptr_t
zImage_get_load_address(void* file, uintptr_t ram_base)
{
    if (image_get_type(file) == IMG_ZIMAGE) {
        struct zimage_hdr* hdr;
        hdr = (struct zimage_hdr*)file;
        if (hdr->start == 0) {
            return ram_base + 0x8000;
        } else {
            return hdr->start;
        }
    }
    return 0;
}

