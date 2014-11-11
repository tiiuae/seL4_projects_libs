/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef SEL4ARM_VMM_EXYNOS_DEVICES_H
#define SEL4ARM_VMM_EXYNOS_DEVICES_H

#include <sel4arm-vmm/exynos/device_map.h>
#include <sel4arm-vmm/vm.h>

extern const struct device dev_vram;
int vm_install_ram_default(vm_t *vm);
int vm_install_ram_range(vm_t *vm, uintptr_t start, size_t size);

int vm_install_vgic(vm_t* vm);
int vm_install_vcombiner(vm_t* vm);

typedef int (*vm_power_cb)(vm_t* vm, void* token);
int vm_install_vpower(vm_t* vm, vm_power_cb shutdown_cb, void* shutdown_token,
                      vm_power_cb reboot_cb, void* reboot_token);

int vm_install_vsysreg(vm_t* vm);

extern const struct device dev_vmct_timer;
int vm_install_vmct(vm_t* vm);

const struct device dev_ps_msh0;
const struct device dev_ps_msh2;
int vm_install_sdhc0(vm_t *vm);
int vm_install_sdhc2(vm_t *vm);

#define dev_vconsole dev_uart2
extern const struct device dev_uart0;
extern const struct device dev_uart1;
extern const struct device dev_uart2;
extern const struct device dev_uart3;

int vm_install_vconsole(vm_t* vm);
void flush_vconsole_device(struct device* d);

extern const struct device dev_acp;
extern const struct device dev_i2c1;
extern const struct device dev_i2c2;
extern const struct device dev_i2c4;
extern const struct device dev_i2chdmi;
extern const struct device dev_usb2_ohci;
extern const struct device dev_usb2_ehci;
extern const struct device dev_usb2_ctrl;
extern const struct device dev_gpio_left;
extern const struct device dev_ps_gpio_right;
extern const struct device dev_ps_alive;
extern const struct device dev_ps_cmu_top;
extern const struct device dev_ps_cmu_core;
extern const struct device dev_ps_chip_id;
extern const struct device dev_ps_cmu_cpu;
extern const struct device dev_ps_cmu_mem;
extern const struct device dev_ps_cmu_cdrex;
extern const struct device dev_ps_cmu_isp;
extern const struct device dev_ps_cmu_acp;
extern const struct device dev_ps_sysreg;
extern const struct device dev_ps_msh0;
extern const struct device dev_ps_msh2;
extern const struct device dev_ps_tx_mixer;
extern const struct device dev_ps_hdmi0;
extern const struct device dev_ps_hdmi1;
extern const struct device dev_ps_hdmi2;
extern const struct device dev_ps_hdmi3;
extern const struct device dev_ps_hdmi4;
extern const struct device dev_ps_hdmi5;
extern const struct device dev_ps_hdmi6;
extern const struct device dev_ps_pdma0;
extern const struct device dev_ps_pdma1;
extern const struct device dev_ps_mdma0;
extern const struct device dev_ps_mdma1;
extern const struct device dev_ps_pwm_timer;
extern const struct device dev_ps_wdt_timer;

#endif /* SEL4ARM_VMM_EXYNOS_DEVICES_H */
