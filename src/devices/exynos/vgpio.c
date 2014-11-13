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

//#define GPIO_DEBUG

#ifdef GPIO_DEBUG
#define DGPIO(...) printf(__VA_ARGS__)
#else
#define DGPIO(...) do{}while(0)
#endif


/****** Left *****/

struct gpio_left_priv {
    vm_t *vm;
    void* regs;
};

static int
handle_vgpio_left_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    struct gpio_left_priv* gpio_left_data = (struct gpio_left_priv*)d->priv;
    volatile uint32_t *reg;
    int offset;

    /* Gather fault information */
    offset = fault->addr - d->pstart;
    reg = (uint32_t*)(gpio_left_data->regs + offset);
    /* Handle the fault */
    reg = (volatile uint32_t*)(gpio_left_data->regs + offset);
    if (fault_is_read(fault)) {
        fault->data = *reg;
        DGPIO("[%s] pc0x%x| r0x%x:0x%x\n", d->name,
              fault->regs.pc,
              fault->addr,
              fault->data);

    } else {
        DGPIO("[%s] pc0x%x| w0x%x:0x%x\n", d->name,
              fault->regs.pc,
              fault->addr,
              fault->data);
        *reg = fault_emulate(fault, *reg);
    }
    return advance_fault(fault);
}

const struct device dev_gpio_left = {
    .devid = DEV_CUSTOM,
    .name = "gpio.left",
    .pstart = GPIO_LEFT_PADDR,
    .size = 0x1000,
    .handle_page_fault = &handle_vgpio_left_fault,
    .priv = NULL
};

int
vm_install_vgpio_left(vm_t* vm)
{
    struct gpio_left_priv *gpio_left_data;
    struct device d;
    vspace_t* vmm_vspace;
    int err;

    d = dev_gpio_left;
    vmm_vspace = vm->vmm_vspace;

    /* Initialise the virtual device */
    gpio_left_data = malloc(sizeof(struct gpio_left_priv));
    if (gpio_left_data == NULL) {
        assert(gpio_left_data);
        return -1;
    }
    memset(gpio_left_data, 0, sizeof(*gpio_left_data));
    gpio_left_data->vm = vm;

    gpio_left_data->regs = map_device(vmm_vspace, vm->vka, vm->simple,
                                      d.pstart, 0, seL4_AllRights);
    if (gpio_left_data->regs == NULL) {
        return -1;
    }

    d.priv = gpio_left_data;
    err = vm_add_device(vm, &d);
    assert(!err);
    if (err) {
        free(gpio_left_data);
        return -1;
    }

    return 0;
}
/***** Right *****/

struct gpio_right_priv {
    vm_t *vm;
    void* regs;
};

static int
handle_vgpio_right_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    struct gpio_right_priv* gpio_right_data = (struct gpio_right_priv*)d->priv;
    volatile uint32_t *reg;
    int offset;

    /* Gather fault information */
    offset = fault->addr - d->pstart;
    reg = (uint32_t*)(gpio_right_data->regs + offset);
    /* Handle the fault */
    reg = (volatile uint32_t*)(gpio_right_data->regs + offset);
    if (fault_is_read(fault)) {
        fault->data = *reg;
        DGPIO("[%s] pc0x%x| r0x%x:0x%x\n", d->name,
              fault->regs.pc,
              fault->addr,
              fault->data);

    } else {
        DGPIO("[%s] pc0x%x| w0x%x:0x%x\n", d->name,
              fault->regs.pc,
              fault->addr,
              fault->data);
        *reg = fault_emulate(fault, *reg);
    }
    return advance_fault(fault);
}

const struct device dev_gpio_right = {
    .devid = DEV_CUSTOM,
    .name = "gpio_right",
    .pstart = GPIO_RIGHT_PADDR,
    .size = 0x1000,
    .handle_page_fault = &handle_vgpio_right_fault,
    .priv = NULL
};

int
vm_install_vgpio_right(vm_t* vm)
{
    struct gpio_right_priv *gpio_right_data;
    struct device d;
    vspace_t* vmm_vspace;
    int err;

    d = dev_gpio_right;
    vmm_vspace = vm->vmm_vspace;

    /* Initialise the virtual device */
    gpio_right_data = malloc(sizeof(struct gpio_right_priv));
    if (gpio_right_data == NULL) {
        assert(gpio_right_data);
        return -1;
    }
    memset(gpio_right_data, 0, sizeof(*gpio_right_data));
    gpio_right_data->vm = vm;

    gpio_right_data->regs = map_device(vmm_vspace, vm->vka, vm->simple,
                                       d.pstart, 0, seL4_AllRights);
    if (gpio_right_data->regs == NULL) {
        return -1;
    }

    d.priv = gpio_right_data;
    err = vm_add_device(vm, &d);
    assert(!err);
    if (err) {
        free(gpio_right_data);
        return -1;
    }

    return 0;
}

