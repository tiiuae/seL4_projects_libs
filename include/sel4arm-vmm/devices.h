/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef SEL4ARM_VMM_DEVICES_H
#define SEL4ARM_VMM_DEVICES_H

#include <stdint.h>

#include <sel4arm-vmm/fault.h>

typedef struct vm vm_t;

/**
 * Logical device identifier
 */
enum devid {
    DEV_RAM,
    DEV_VGIC_DIST,
    DEV_VGIC_CPU,
    DEV_VGIC_VCPU,
    DEV_PWM_TIMER,
    DEV_WDT_TIMER,
    DEV_MCT_TIMER,
    DEV_UART0,
    DEV_UART1,
    DEV_UART2,
    DEV_UART3,
    /* Number of devices */
    DEV_NDEVICES,
    /* Special values */
    DEV_CUSTOM,
    /* Aliases */
    DEV_CONSOLE = DEV_UART3
};


/**
 * Device description
 */
struct device {
/// Logical identifier for internal use
    enum devid devid;
/// A string representation of the device. Useful for debugging
    const char* name;
/// The physical address of the device */
    uint32_t pstart;
/// Device mapping size */
    uint32_t size;
/// Fault handler */
    int (*handle_page_fault)(struct device* d, vm_t* vm, fault_t* fault);
/// device emulation private data */
    void* priv;
};

/**
 * Install a passthrough device into a VM
 * @param[in] vm     A handle to the VM that the device should be install to
 * @param[in] device A description of the device
 * @return           0 on success
 */
int vm_install_passthrough_device(vm_t* vm, const struct device* device);


/**
 * Install a passthrough device into a VM, but trap and print all access
 * @param[in] vm     A handle to the VM that the device should be install to
 * @param[in] device A description of the device
 * @return           0 on success
 */
int vm_install_listening_device(vm_t* vm, const struct device* device);

/**
 * Install a device into the VM and call its associated fault handler on access
 * @param[in] vm     A handle to the VM that the device should be install to
 * @param[in] device A description of the device
 * @return           0 on success
 */
int vm_install_emulated_device(vm_t* vm, const struct device *device);


/**
 * Add a generic device to the VM without performind any initialisation of the device
 * When the VM receives a fault with an address that is in the range of this device,
 * it will call the assigned fault handler.
 * @param[in] vm     A handle to the VM that the device should be install to
 * @param[in] device A description of the device
 * @return           0 on success
 */
int vm_add_device(vm_t* vm, const struct device* d);


#endif /* SEL4ARM_VMM_DEVICES_H */
