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

#include <stdint.h>
#include <sel4vm/guest_vm.h>

/* Device Management Object */
struct device {
    /* A string representation of the device. Useful for debugging */
    const char* name;
    /* The physical address of the device */
    seL4_Word pstart;
    /* Device mapping size */
    seL4_Word size;
    /* Fault handler */
    int (*handle_device_fault)(vm_t* vm, uint32_t addr, uint32_t *data, size_t len, bool is_write, void *priv);
    /* Device emulation private data */
    void* priv;
};

/*
 * Management for a list of devices
 */
typedef struct device_list {
    struct device *devices;
    int num_devices;
} device_list_t;

/*
 * Initialise an empty device list
 * @param list      device list to initialise
 * @return          0 on success, otherwise -1 for error
 */
int device_list_init(device_list_t *list);

/*
 * Add a generic device to a given device list without performing any initialisation of the device
 * @param dev_list  A handle to the device list that the device should be installed into
 * @param device    A description of the device
 * @return          0 on success, otherwise -1 for error
 */
int add_device(device_list_t *dev_list, const struct device* d);

/*
 * Find a device by a given addr within a device list
 * @param dev_list  Device list to search within
 * @param addr      Add to search with
 * @return          Pointer to device if found, otherwise NULL if not found
 */
struct device* find_device_by_pa(device_list_t *dev_list, uintptr_t addr);
