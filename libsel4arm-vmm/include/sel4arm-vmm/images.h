/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef SEL4ARM_VMM_IMAGES_H
#define SEL4ARM_VMM_IMAGES_H

#include <stdint.h>


enum img_type {
/// Binary file, or unknown type
    IMG_BIN,
/// Elf file
    IMG_ELF,
/// u-boot image
    IMG_UIMAGE,
/// Android boot image
    IMG_ABOOT,
/// Self decompressing linux image file
    IMG_ZIMAGE,
/// Flattened device tree blob
    IMG_DTB
};

/**
 * Retrieve the file type of a file in memory
 * @param[in] file  The location, in memory, of the file
 * @return          The image type. IMG_BIN is return if the type could
 *                  not be identified
 */
enum img_type image_get_type(void* file);

/**
 * Retrieve the load address of a zImage from the zImage header
 * @param[in] file      The location, in memory, of the file
 * @param[in] ram_base  A hint for the first address of RAM
 * @return              0 on error, otherwise, returns the load address of the
 *                      zImage
 */
uintptr_t zImage_get_load_address(void* file, uintptr_t ram_base);

#endif /* SEL4ARM_VMM_IMAGES_H */
