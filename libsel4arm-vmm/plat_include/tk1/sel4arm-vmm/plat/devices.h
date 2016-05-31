/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef SEL4ARM_VMM_TK1_DEVICES_H
#define SEL4ARM_VMM_TK1_DEVICES_H

#include <sel4arm-vmm/plat/device_map.h>
#include <sel4arm-vmm/vm.h>

#define GIC_PADDR   0x50040000
#define MAX_VIRQS   200

extern const struct device dev_vram;


typedef int (*vm_power_cb)(vm_t* vm, void* token);
int vm_install_vpower(vm_t* vm, vm_power_cb shutdown_cb, void* shutdown_token,
                      vm_power_cb reboot_cb, void* reboot_token);

#endif /* SEL4ARM_VMM_EXYNOS_DEVICES_H */
