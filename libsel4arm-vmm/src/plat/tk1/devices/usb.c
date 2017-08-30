/*
* Copyright 2017, Data61
* Commonwealth Scientific and Industrial Research Organisation (CSIRO)
* ABN 41 687 119 230.
* This software may be distributed and modified according to the terms of
* the BSD 2-Clause license. Note that NO WARRANTY is provided.
* See "LICENSE_BSD2.txt" for details.
* @TAG(DATA61_BSD)
*/


#include <stdlib.h>
#include <string.h>
#include <sel4/types.h>

#include <sel4arm-vmm/devices.h>
#include <utils/io.h>

#include "../../../devices.h"
#include "../../../vm.h"

#define USB2_CONTROLLER_USB2D_USBCMD_0_OFFSET 0x130
#define USB2D_USB_COMMAND_REGISTER_RESET_BIT BIT(1)

const struct device dev_usb = {
    .devid = DEV_CUSTOM,
    .name = "usb",
    .pstart = 0x7d004000,
    .size = PAGE_SIZE,
    .handle_page_fault = NULL,
    .priv = NULL
};

static int usb_vm_reboot_hook(vm_t* vm, void *token) {
    void *usb_cmd_register = token + USB2_CONTROLLER_USB2D_USBCMD_0_OFFSET;
    uint32_t reg = RAW_READ32(usb_cmd_register);
    RAW_WRITE32(reg | USB2D_USB_COMMAND_REGISTER_RESET_BIT, usb_cmd_register);
    return 0;
}



int
vm_install_tk1_usb_passthrough_device(vm_t* vm)
{

    /* Add the device */
    void *addr = map_vm_device(vm, dev_usb.pstart, dev_usb.pstart, seL4_AllRights);
    if (addr == NULL) {
        ZF_LOGE("map_vm_device returned NULL");
        return -1;
    }
    void *vmm_addr = vspace_share_mem(vm_get_vspace(vm), vm_get_vmm_vspace(vm), addr, 1,
    seL4_PageBits, seL4_AllRights, 0);
    if (vmm_addr == NULL) {
        ZF_LOGE("vspace_share_mem returned NULL");
        return -1;
    }

    int err = vm_add_device(vm, &dev_usb);
    if (err) {
        ZF_LOGE("vm_add_device returned error: %d", err);
        return -1;
    }
    err = vm_register_reboot_callback(vm, usb_vm_reboot_hook, vmm_addr);
    if (err) {
        ZF_LOGE("vm_register_reboot_callback returned error: %d", err);
        return -1;
    }

    return 0;
}
