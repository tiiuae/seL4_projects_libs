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
#include <sel4vmmplatsupport/arch/guest_image_arch.h>

typedef struct guest_kernel_image_arch guest_kernel_image_arch_t;

typedef struct guest_image {
    /* Base address (in guest physical) where the image was loaded */
    uintptr_t load_paddr;
    /* Alignment we used when loading the image */
    size_t alignment;
    /* Size of guest image */
    size_t size;
} guest_image_t;

/* Stores information about the guest kernel image we are loading. This information probably stops
 * being relevant / useful after we start running. */
typedef struct guest_kernel_image {
    guest_image_t kernel_image;
    /* Architecture specific information for loaded guest image */
    guest_kernel_image_arch_t kernel_image_arch;
} guest_kernel_image_t;

/* Load guest kernel image
 * @param vm            Handle to the VM
 * @param kernel_name   Name of the kernel image
 * @param load_address  Address to load guest kernel image at
 * @param alignment     Alignment for loading kernel image
 * @param guest_kernel_image   Handle to information regarding the resulted loading of the guest kernel image
 * @return              0 on success, otherwise -1 on error
 */
int vm_load_guest_kernel(vm_t *vm, const char *kernel_name, uintptr_t load_address, size_t alignment, guest_kernel_image_t *guest_kernel_image);

/* Load guest kernel module e.g. initrd
 * @param vm            Handle to the VM
 * @param module_name   Name of the module image
 * @param load_address  Address to load guest kernel image at
 * @param alignment     Alignment for loading module image
 * @param guest_image   Handle to information regarding the resulted loading of the guest module image
 * @return              0 on success, otherwise -1 on error
 */
int vm_load_guest_module(vm_t *vm, const char *module_name, uintptr_t load_address, size_t alignment, guest_image_t *guest_image);
