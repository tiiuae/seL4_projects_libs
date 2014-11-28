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
#include <platsupport/gpio.h>

//#define GPIO_DEBUG

#ifdef GPIO_DEBUG
#define DGPIO(...) printf(__VA_ARGS__)
#else
#define DGPIO(...) do{}while(0)
#endif


#define GPIOREG_CON_OFFSET    0x00
#define GPIOREG_DAT_OFFSET    0x04
#define GPIOREG_PUD_OFFSET    0x08
#define GPIOREG_DRV_OFFSET    0x0C
#define GPIOREG_CONPDN_OFFSET 0x10
#define GPIOREG_PUDPDN_OFFSET 0x14
#define GPIOREG_SIZE          0x20
#define GPIOREG_OFFSET_MASK   ((GPIOREG_SIZE - 1) & ~MASK(2))

#define GPIOREG_CON_BITS    4
#define GPIOREG_DAT_BITS    1
#define GPIOREG_PUD_BITS    2
#define GPIOREG_DRV_BITS    2
#define GPIOREG_CONPDN_BITS 2
#define GPIOREG_PUDPDN_BITS 2

#define NPORTS_PER_BANK (0x1000 / GPIOREG_SIZE)
#define NPINS_PER_PORT  (8)
#define NPINS_PER_BANK (NPORTS_PER_BANK * NPINS_PER_PORT)

static int handle_vgpio_right_fault(struct device* d, vm_t* vm, fault_t* fault);
static int handle_vgpio_left_fault(struct device* d, vm_t* vm, fault_t* fault);

struct gpio_device {
    vm_t *vm;
    enum vacdev_action action;
    void* regs[GPIO_NBANKS];
    uint8_t granted_bf[GPIO_NBANKS][((NPINS_PER_BANK - 1 / 8) + 1)];
};

static const struct device* gpio_devices[] = {
    [GPIO_LEFT_BANK]  = &dev_gpio_left,
    [GPIO_RIGHT_BANK] = &dev_gpio_right,
    [GPIO_C2C_BANK]   = NULL,
    [GPIO_AUDIO_BANK] = NULL,
};


const struct device dev_gpio_left = {
    .devid = DEV_CUSTOM,
    .name = "gpio.left",
    .pstart = GPIO_LEFT_PADDR,
    .size = 0x1000,
    .handle_page_fault = &handle_vgpio_left_fault,
    .priv = NULL
};

const struct device dev_gpio_right = {
    .devid = DEV_CUSTOM,
    .name = "gpio.right",
    .pstart = GPIO_RIGHT_PADDR,
    .size = 0x1000,
    .handle_page_fault = &handle_vgpio_right_fault,
    .priv = NULL
};

static uint32_t
_create_mask(uint8_t ac, int bits)
{
    uint32_t mask = 0;
    while (ac) {
        int pin = CTZ(ac);
        ac &= ~BIT(pin);
        mask |= MASK(bits) << (bits * pin);
    }
    return mask;
}

static int
handle_vgpio_fault(struct device* d, vm_t* vm, fault_t* fault, int bank)
{
    struct gpio_device* gpio_device = (struct gpio_device*)d->priv;
    volatile uint32_t *reg;
    int offset;

    /* Gather fault information */
    offset = fault->addr - d->pstart;
    reg = (volatile uint32_t*)(gpio_device->regs[bank] + offset);
    if (fault_is_read(fault)) {
        fault->data = *reg;
        DGPIO("[%s] pc 0x%08x | r 0x%08x:0x%08x\n", gpio_devices[bank]->name,
              fault->regs.pc, fault->addr, fault->data);
    } else {
        uint32_t mask;
        uint32_t change;
        DGPIO("[%s] pc 0x%08x | w 0x%08x:0x%08x\n", gpio_devices[bank]->name,
              fault->regs.pc, fault->addr, fault->data);
        if ((offset >= 0x700 && offset < 0xC00) || offset >= 0xE00) {
            /* Not implemented */
            mask = 0xFFFFFFFF;
        } else {
            /* GPIO and MUX general registers */
            uint32_t ac;
            int port;
            port = offset / GPIOREG_SIZE;
            assert(port < sizeof(gpio_device->granted_bf[bank]));
            ac = gpio_device->granted_bf[bank][port];
            switch (offset & GPIOREG_OFFSET_MASK) {
            case GPIOREG_CON_OFFSET:
                mask = _create_mask(ac, GPIOREG_CON_BITS);
                break;
            case GPIOREG_DAT_OFFSET:
                mask = _create_mask(ac, GPIOREG_DAT_BITS);
                break;
            case GPIOREG_PUD_OFFSET:
                mask = _create_mask(ac, GPIOREG_PUD_BITS);
                break;
            case GPIOREG_DRV_OFFSET:
                mask = _create_mask(ac, GPIOREG_DRV_BITS);
                break;
            case GPIOREG_CONPDN_OFFSET:
                mask = _create_mask(ac, GPIOREG_CONPDN_BITS);
                break;
            case GPIOREG_PUDPDN_OFFSET:
                mask = _create_mask(ac, GPIOREG_PUDPDN_BITS);
                break;
            default:  /* reserved */
                printf("reserved GPIO 0x%x\n", offset);
                mask = 0;
            }
        }
        change = fault_emulate(fault, *reg);
        if ((change ^ *reg) & ~mask) {
            switch (gpio_device->action) {
            case VACDEV_REPORT_AND_MASK:
                change = (change & mask) | (*reg & ~mask);
            case VACDEV_REPORT_ONLY:
                /* Fallthrough */
                printf("[%s] pc 0x%08x| w @ 0x%08x: 0x%08x -> 0x%08x\n", gpio_devices[bank]->name,
                       fault->regs.pc, fault->addr, *reg, change);
                break;
            default:
                break;
            }
        }
        *reg = change;
    }
    return advance_fault(fault);
}

static int
handle_vgpio_right_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    return handle_vgpio_fault(d, vm, fault, GPIO_RIGHT_BANK);
}

static int
handle_vgpio_left_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    return handle_vgpio_fault(d, vm, fault, GPIO_LEFT_BANK);
}


struct gpio_device*
vm_install_ac_gpio(vm_t* vm, enum vacdev_default default_ac, enum vacdev_action action) {
    struct gpio_device* gpio_device;
    vspace_t* vmm_vspace;
    vmm_vspace = vm->vmm_vspace;
    uint8_t ac = (default_ac == VACDEV_DEFAULT_ALLOW) ? 0xff : 0x00;
    int i;

    gpio_device = (struct gpio_device*)malloc(sizeof(*gpio_device));
    if (gpio_device == NULL) {
        return NULL;
    }
    gpio_device->vm = vm;
    gpio_device->action = action;
    memset(gpio_device->granted_bf, ac, sizeof(gpio_device->granted_bf));
    for (i = 0; i < GPIO_NBANKS; i++) {
        /* Map the device locally */
        if (gpio_devices[i] != NULL) {
            struct device dev;
            int err;
            dev = *gpio_devices[i];
            dev.priv = gpio_device;
            gpio_device->regs[i] = map_device(vmm_vspace, vm->vka, vm->simple,
                                              dev.pstart, 0, seL4_AllRights);
            /* Ignore failues. We will check regs for NULL on access */
            err = vm_add_device(vm, &dev);
            assert(!err);
            if (err) {
                return NULL;
            }
        }
    }
    return gpio_device;
}

static int
vm_gpio_config(struct gpio_device* gpio_device, gpio_id_t gpio_id, int provide)
{
    int gpioport;
    int port, pin, bank;
    gpioport = GPIOID_PORT(gpio_id);
    bank = GPIOPORT_GET_BANK(gpioport);
    port = GPIOPORT_GET_PORT(gpioport);
    pin = GPIOID_PIN(gpio_id);

    if (provide) {
        gpio_device->granted_bf[bank][port] |= BIT(pin);
    } else {
        gpio_device->granted_bf[bank][port] &= ~BIT(pin);
    }
    return 0;
}

int
vm_gpio_provide(struct gpio_device* gpio_device, gpio_id_t gpio_id)
{
    return vm_gpio_config(gpio_device, gpio_id, 1);
}

int
vm_gpio_restrict(struct gpio_device* gpio_device, gpio_id_t gpio_id)
{
    return vm_gpio_config(gpio_device, gpio_id, 0);
}

