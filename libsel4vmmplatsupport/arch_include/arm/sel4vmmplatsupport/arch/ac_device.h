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

#include <sel4vmmplatsupport/device.h>

enum vacdev_default {
    VACDEV_DEFAULT_ALLOW,
    VACDEV_DEFAULT_DENY
};

enum vacdev_action {
    VACDEV_REPORT_ONLY,
    VACDEV_MASK_ONLY,
    VACDEV_REPORT_AND_MASK
};

/**
 * Installs a generic access controlled device
 * @param[in] vm     The VM to install the device into
 * @param[in] d      A description of the device to install
 * @param[in] mask   An access mask. The mask provides a map of device bits that
 *                   are modifiable by the guest.
 *                   '1' represents bits that the guest can read and write
 *                   '0' represents bits that can only be read by the guest
 *                   Underlying memory for the mask should remain accessible for
 *                   the life of this device. The mask may be updated at run time
 *                   on demand.
 * @param[in] size   The size of the mask. This is useful for conserving memory in
 *                   cases where the underlying device does not occupy a full
 *                   page. If an access lies outside of the range of the mask,
 *                   guest access.
 * @param[in] action Action to take when access is violated.
 * @return           0 on success
 */
int vm_install_generic_ac_device(vm_t *vm, const struct device *d, void *mask,
                                 size_t size, enum vacdev_action action);
