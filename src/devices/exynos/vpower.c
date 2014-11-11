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

struct power_priv {
    vm_t *vm;
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
    vm_offset = fault->addr - d->pstart;
    bank = vm_offset >> 12;
    offset = vm_offset & MASK(12);
    reg_offset = offset & ~MASK(2);

    /* Handle the fault */
    reg = (volatile uint32_t*)(power_data->regs[bank] + offset);
    if (fault_is_read(fault)) {
        fault->data = *reg;
        DPWR("[%s] pc0x%x| r0x%x:0x%x\n", d->name,
             fault->regs.pc,
             fault->addr,
             fault->data);

    } else {
        if (bank == PWR_SWRST_OFFSET && reg_offset == PWR_SWRST_OFFSET) {
            if (fault->data) {
                /* Software reset */
                DPWR("[%s] Software reset\n", d->name);
            }
        } else {
            DPWR("[%s] pc0x%x| w0x%x:0x%x\n", d->name,
                 fault->regs.pc,
                 fault->addr,
                 fault->data);
            *reg = fault_emulate(fault, *reg);
        }
    }
    return advance_fault(fault);
}

const struct device dev_valive = {
    .devid = DEV_CUSTOM,
    .name = "alive",
    .pstart = ALIVE_PADDR,
    .size = 0x5000,
    .handle_page_fault = &handle_vpower_fault,
    .priv = NULL
};

int
vm_install_vpower(vm_t* vm)
{
    struct power_priv *power_data;
    struct device d;
    vspace_t* vmm_vspace;
    int err;
    int i;

    d = dev_valive;
    vmm_vspace = vm->vmm_vspace;

    /* Initialise the virtual device */
    power_data = malloc(sizeof(struct power_priv));
    if (power_data == NULL) {
        assert(power_data);
        return -1;
    }
    memset(power_data, 0, sizeof(*power_data));
    power_data->vm = vm;

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

