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
#include <stdlib.h>
#include <string.h>

#include <sel4vm/guest_vm.h>

#include "../../../devices.h"
#include "../../../vm.h"

//#define SYSREG_DEBUG

#ifdef SYSREG_DEBUG
#define DSYSREG(...) printf(__VA_ARGS__)
#else
#define DSYSREG(...) do{}while(0)
#endif



struct sysreg_priv {
    struct dma *dma_list;
    vm_t *vm;
    void *regs;
};

static int handle_vsysreg_fault(struct device *d, vm_t *vm, fault_t *fault)
{
    struct sysreg_priv *sysreg_data = (struct sysreg_priv *)d->priv;
    volatile uint32_t *reg;
    int offset;

    /* Gather fault information */
    offset = fault_get_address(fault) - d->pstart;
    reg = (uint32_t *)(sysreg_data->regs + offset);
    /* Handle the fault */
    reg = (volatile uint32_t *)(sysreg_data->regs + offset);
    if (fault_is_read(fault)) {
        fault_set_data(fault, *reg);
        DSYSREG("[%s] pc0x%x| r0x%x:0x%x\n", d->name, fault_get_ctx(fault)->pc,
                fault_get_address(fault), fault_get_data(fault));
    } else {
        DSYSREG("[%s] pc0x%x| w0x%x:0x%x\n", d->name, fault_get_ctx(fault)->pc,
                fault_get_address(fault), fault_get_data(fault));
        *reg = fault_emulate(fault, *reg);
    }
    return advance_fault(fault);
}

const struct device dev_sysreg = {
    .devid = DEV_CUSTOM,
    .name = "sysreg",
    .pstart = SYSREG_PADDR,
    .size = 0x1000,
    .handle_page_fault = &handle_vsysreg_fault,
    .priv = NULL
};

int vm_install_vsysreg(vm_t *vm)
{
    struct sysreg_priv *sysreg_data;
    struct device d;
    vspace_t *vmm_vspace;
    int err;

    d = dev_sysreg;
    vmm_vspace = &vm->mem.vmm_vspace;

    /* Initialise the virtual device */
    sysreg_data = malloc(sizeof(struct sysreg_priv));
    if (sysreg_data == NULL) {
        assert(sysreg_data);
        return -1;
    }
    memset(sysreg_data, 0, sizeof(*sysreg_data));
    sysreg_data->vm = vm;

    sysreg_data->regs = map_device(vmm_vspace, vm->vka, vm->simple,
                                   d.pstart, 0, seL4_AllRights);
    if (sysreg_data->regs == NULL) {
        return -1;
    }

    d.priv = sysreg_data;
    err = vm_add_device(vm, &d);
    assert(!err);
    if (err) {
        free(sysreg_data);
        return -1;
    }

    return 0;
}
