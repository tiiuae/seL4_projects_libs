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

/**
 * Install a passthrough device into a VM
 * @param[in] vm     A handle to the VM that the device should be install to
 * @param[in] device A description of the device
 * @return           0 on success
 */
int vm_install_passthrough_device(vm_t* vm, const struct device* device);

/**
 * Install a device backed by ram into a VM
 * @param[in] vm     A handle to the VM that the device should be install to
 * @param[in] device A description of the device
 * @return           0 on success
 */
int vm_install_ram_only_device(vm_t *vm, const struct device* device);

/**
 * Install a passthrough device into a VM, but trap and print all access
 * @param[in] vm     A handle to the VM that the device should be install to
 * @param[in] device A description of the device
 * @return           0 on success
 */
int vm_install_listening_device(vm_t* vm, const struct device* device);
