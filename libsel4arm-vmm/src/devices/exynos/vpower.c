/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdlib.h>
#include <string.h>

#include "../../devices.h"
#include "../../vm.h"

//#define PWR_DEBUG

#ifdef PWR_DEBUG
#define DPWR(...) printf(__VA_ARGS__)
#else
#define DPWR(...) do{}while(0)
#endif

#define PWR_SWRST_BANK    0
#define PWR_SWRST_OFFSET  0x400

#define PWR_SHUTDOWN_BANK 3
#define PWR_SHUTDOWN_OFFSET 0x30C

struct power_priv {
    vm_t *vm;
    vm_power_cb shutdown_cb;
    void* shutdown_token;
    vm_power_cb reboot_cb;
    void* reboot_token;
    void* regs[5];
};


static int
handle_vpower_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    struct power_priv* power_data = (struct power_priv*)d->priv;
    volatile uint32_t *reg;
    int vm_offset, offset, reg_offset;
    int bank;

    /* Gather fault information */
    vm_offset = fault_get_address(fault) - d->pstart;
    bank = vm_offset >> 12;
    offset = vm_offset & MASK(12);
    reg_offset = offset & ~MASK(2);

    /* Handle the fault */
    reg = (volatile uint32_t*)(power_data->regs[bank] + offset);
    if (fault_is_read(fault)) {
        fault_set_data(fault, *reg);
        DPWR("[%s] pc0x%x| r0x%x:0x%x\n", d->name, fault_get_ctx(fault)->pc,
             fault_get_address(fault), fault_get_data(fault));
    } else {
        if (bank == PWR_SWRST_BANK && reg_offset == PWR_SWRST_OFFSET) {
            if (fault_get_data(fault)) {
                /* Software reset */
                DPWR("[%s] Software reset\n", d->name);
                if (power_data->reboot_cb) {
                    int err;
                    err = power_data->reboot_cb(vm, power_data->reboot_token);
                    if (err) {
                        return err;
                    }
                }
            }
        } else if (bank == PWR_SHUTDOWN_BANK && reg_offset == PWR_SHUTDOWN_OFFSET) {
            uint32_t new_reg = fault_emulate(fault, *reg);
            new_reg &= BIT(31) | BIT(9) | BIT(8);
            if (new_reg == (BIT(31) | BIT(9))) {
                /* Software power down */
                DPWR("[%s] Power down\n", d->name);
                if (power_data->shutdown_cb) {
                    int err;
                    err = power_data->shutdown_cb(vm, power_data->reboot_token);
                    if (err) {
                        return err;
                    }
                }
            } else {
                *reg = fault_emulate(fault, *reg);
            }

        } else {
            DPWR("[%s] pc 0x%x| access violation writing 0x%x to 0x%x\n",
                 d->name, fault_get_ctx(fault)->pc, fault_get_data(fault),
                 fault_get_address(fault));
        }
    }
    return advance_fault(fault);
}

const struct device dev_alive = {
    .devid = DEV_CUSTOM,
    .name = "alive",
    .pstart = ALIVE_PADDR,
    .size = 0x5000,
    .handle_page_fault = &handle_vpower_fault,
    .priv = NULL
};

int
vm_install_vpower(vm_t* vm, vm_power_cb shutdown_cb, void* shutdown_token,
                  vm_power_cb reboot_cb, void* reboot_token)
{
    struct power_priv *power_data;
    struct device d;
    vspace_t* vmm_vspace;
    int err;
    int i;

    d = dev_alive;
    vmm_vspace = vm->vmm_vspace;

    /* Initialise the virtual device */
    power_data = malloc(sizeof(struct power_priv));
    if (power_data == NULL) {
        assert(power_data);
        return -1;
    }
    memset(power_data, 0, sizeof(*power_data));
    power_data->vm = vm;
    power_data->shutdown_cb = shutdown_cb;
    power_data->shutdown_token = shutdown_token;
    power_data->reboot_cb = reboot_cb;
    power_data->reboot_token = reboot_token;

    for (i = 0; i < d.size >> 12; i++) {
        power_data->regs[i] = map_device(vmm_vspace, vm->vka, vm->simple,
                                         d.pstart + (i << 12), 0, seL4_AllRights);
        if (power_data->regs[i] == NULL) {
            return -1;
        }
    }

    d.priv = power_data;
    err = vm_add_device(vm, &d);
    assert(!err);
    if (err) {
        free(power_data);
        return -1;
    }

    return 0;
}

