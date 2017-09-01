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
#ifndef SEL4ARM_VMM_DEVICES_VUSB_H
#define SEL4ARM_VMM_DEVICES_VUSB_H

#include <autoconf.h>
#ifdef CONFIG_LIB_USB

#include <usb/usb_host.h>
#include <sel4arm-vmm/vm.h>
#include <sel4arm-vmm/devices.h>
#include <sel4/sel4.h>

typedef struct vusb_device vusb_device_t;
/**
 * Install a virtual usb device
 * @param[in] vm       The VM in which to install the device
 * @param[in] hcd      The USB host controller that should be used for USB
 *                     transactions. Calls made to this hcd may be redirected
 *                     for filtering.
 * @param[in] pbase    The guest physical address of the device (2 pages)
 * @param[in] virq     The virtual IRQ number for this device
 * @param[in] vmm_ncap The capability to the endpoint at which the VMM waits
 *                     for notifications.
 * @param[in] vm_ncap  The index at which to install a notification capability
 *                     into the VM
 * @param[in] badge    The seL4 badge which should be applied to the
 *                     notification capability.
 * @return             A handle to the virtual usb device, or NULL on failure
 */
vusb_device_t* vm_install_vusb(vm_t* vm, usb_host_t* hcd, uintptr_t pbase,
                               int virq, seL4_CPtr vmm_ncap, seL4_CPtr vm_ncap,
                               int badge);


/**
 * This function should be called when a notification is received from the
 * VM. The notification is identifyable by a message on the fault endpoint
 * of the VM which has a badge that matches that which was passed into the
 * vm_install_vusb function.
 * @param[in] vusb  A handle to a virtual usb device
 */
void vm_vusb_notify(vusb_device_t* vusb);

#endif /* CONFIG_LIB_USB */
#endif /* SEL4ARM_VMM_DEVICES_VUSB_H */
