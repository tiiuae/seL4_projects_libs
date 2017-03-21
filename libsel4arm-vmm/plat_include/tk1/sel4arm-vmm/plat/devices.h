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


#define dev_vconsole dev_uartd

extern const struct device dev_uartd;
extern const struct device dev_clkcar;
extern const struct device dev_usb1;
extern const struct device dev_usb3;
extern const struct device dev_sdmmc;
extern const struct device dev_rtc_kbc_pmc;
extern const struct device dev_data_memory;
extern const struct device dev_exception_vectors;
extern const struct device dev_system_registers;
extern const struct device dev_ictlr;
extern const struct device dev_apb_misc;
extern const struct device dev_fuse;
extern const struct device dev_gpios;

typedef int (*vm_power_cb)(vm_t* vm, void* token);
int vm_install_vpower(vm_t* vm, vm_power_cb shutdown_cb, void* shutdown_token,
                      vm_power_cb reboot_cb, void* reboot_token);

int vm_install_tk1_usb_passthrough_device(vm_t *vm);

#endif /* SEL4ARM_VMM_EXYNOS_DEVICES_H */
