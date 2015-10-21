/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef SEL4ARM_VMM_ATAGS_H
#define SEL4ARM_VMM_ATAGS_H

#include <stdint.h>

#define ATAG_NONE       0x00000000
#define ATAG_CORE       0x54410001
#define ATAG_MEM        0x54410002
#define ATAG_VIDEOTEXT  0x54410003
#define ATAG_RAMDISK    0x54410004
#define ATAG_INITRD2    0x54420005
#define ATAG_SERIAL     0x54410006
#define ATAG_REVISION   0x54410007
#define ATAG_VIDEOLFB   0x54410008
#define ATAG_CMDLINE    0x54410009

/**
 * Structure for a single atag. The context of the data is sensative to
 * the assigned tag
 */
struct atag_hdr {
/// The size, in words, of the atag
    uint32_t size;
/// The tag identifier of the atag
    uint32_t tag;
/// Context sensitive atag data.
    char     data[];
};

/**
 * Maintains a list of atags. The list may not be contiguous in memory
 */
struct atag_list {
/// The location of the atag in memory
    struct atag_hdr* hdr;
/// Next atag in the list
    struct atag_list* next;
};

/**
 * Initialise a list of atags
 * @return An initialised list of atags.
 *         The list Will contain a structureless CORE tag
 */
struct atag_list* atags_new(void);

/**
 * Retrieve the size of an atag in bytes
 * @param[in] atag  The atag in question
 * @return          The size (in bytes) of the provided atag
 */
int atags_size_bytes(struct atag_list* atag);

/**
 * Adds the CORE atag.
 * A valid list must contain a single core tag at the
 * beggining of the list. This tag was already created in atags_new so at
 * this point, we are just updating or adding data.
 * @param[in] atags    The atags list
 * @param[in] flags    atag member data: Flags
 * @param[in] pagesize atag member data: page size (page size of the system)
 * @param[in] rootdev  atag member data: The device that hold the root filesystem
 * @return             0 on success
 */
int atags_add_core(struct atag_list* atags,
                   uint32_t flags, uint32_t pagesize, uint32_t rootdev);

/**
 * Appends a string to the boot command line. If an atag for the command
 * line does not yet exist, one will be created.
 * @param[in] atags    The atags list
 * @param[in] arg      a string to add to the command line arguments
 * @return             0 on success
 */
int atags_append_cmdline(struct atag_list* atags, const char* arg);

/**
 * Adds a memory region to the atags
 * @param[in] atags    The atags list
 * @param[in] size     atag member data: the size of the memory region
 * @param[in] start    atag member data: The start address of memory
 * @return             0 on success
 */
int atags_add_mem(struct atag_list* atags, uint32_t size, uint32_t start);


#endif /* SEL4ARM_VMM_ATAGS_H */
