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
#pragma once

#include <sel4arm-vmm/plat/device_map.h>
#include <sel4arm-vmm/vm.h>

#define GIC_PADDR   0x10480000
#define MAX_VIRQS   200

extern const struct device dev_vram;

/**
 * vm_combiner_irq_handler should be called when an IRQ combiner IRQ occurs.
 * The caller is responsible for acknowledging the IRQ once this function
 * returns
 */
int vm_install_vcombiner(vm_t *vm);
void vm_combiner_irq_handler(vm_t *vm, int irq);

typedef int (*vm_power_cb)(vm_t *vm, void *token);
int vm_install_vpower(vm_t *vm, vm_power_cb shutdown_cb, void *shutdown_token,
                      vm_power_cb reboot_cb, void *reboot_token);

int vm_install_vsysreg(vm_t *vm);
int vm_install_vcmu_top(vm_t *vm);
int vm_install_vgpio_left(vm_t *vm);
int vm_install_vgpio_right(vm_t *vm);

extern const struct device dev_vmct_timer;
int vm_install_vmct(vm_t *vm);

const struct device dev_msh0;
const struct device dev_msh2;
int vm_install_nodma_sdhc0(vm_t *vm);
int vm_install_nodma_sdhc2(vm_t *vm);

#define dev_vconsole dev_uart2
extern const struct device dev_uart0;
extern const struct device dev_uart1;
extern const struct device dev_uart2;
extern const struct device dev_uart3;

/**
 * Installs a UART device which provides access to FIFO and IRQ
 * control registers, but prevents access to configuration registers
 * @param[in] vm  The vm in which to install the device
 * @param[in] d   A description of the UART device
 * @return        0 on success
 */
int vm_install_ac_uart(vm_t *vm, const struct device *d);

/**
 * Installs the default console device. Characters written to the
 * console are buffered until end of line. The output data will
 * be printed with a background colour to identify the VM
 * @param[in] vm The VM in which to install the vconsole device
 * @return       0 on success
 */
int vm_install_vconsole(vm_t *vm);

extern const struct device dev_acp;
extern const struct device dev_i2c1;
extern const struct device dev_i2c2;
extern const struct device dev_i2c4;
extern const struct device dev_i2chdmi;
extern const struct device dev_usb2_ohci;
extern const struct device dev_usb2_ehci;
extern const struct device dev_usb2_ctrl;
extern const struct device dev_gpio_left;
extern const struct device dev_gpio_right;
extern const struct device dev_alive;
extern const struct device dev_ps_chip_id;
extern const struct device dev_cmu_top;
extern const struct device dev_cmu_core;
extern const struct device dev_cmu_cpu;
extern const struct device dev_cmu_mem;
extern const struct device dev_cmu_cdrex;
extern const struct device dev_cmu_isp;
extern const struct device dev_cmu_acp;
extern const struct device dev_sysreg;
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
