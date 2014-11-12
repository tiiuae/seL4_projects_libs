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

#define CLOCK_DEBUG

#ifdef CLOCK_DEBUG
#define DCLOCK(...) printf(__VA_ARGS__)
#else
#define DCLOCK(...) do{}while(0)
#endif


struct cmu_top_priv {
    struct dma *dma_list;
    vm_t *vm;
    void* regs;
};

static int
handle_vcmu_top_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    struct cmu_top_priv* cmu_top_data = (struct cmu_top_priv*)d->priv;
    volatile uint32_t *reg;
    int offset;

    /* Gather fault information */
    offset = fault->addr - d->pstart;
    reg = (uint32_t*)(cmu_top_data->regs + offset);
    /* Handle the fault */
    reg = (volatile uint32_t*)(cmu_top_data->regs + offset);
    if (fault_is_read(fault)) {
        fault->data = *reg;
        DCLOCK("[%s] pc0x%x| r0x%x:0x%x\n", d->name,
               fault->regs.pc,
               fault->addr,
               fault->data);

    } else {
        DCLOCK("[%s] pc0x%x| w0x%x:0x%x\n", d->name,
               fault->regs.pc,
               fault->addr,
               fault->data);
        *reg = fault_emulate(fault, *reg);
    }
    return advance_fault(fault);
}

const struct device dev_cmu_top = {
    .devid = DEV_CUSTOM,
    .name = "CMU_TOP",
    .pstart = CMU_TOP_PADDR,
    .size = 0x1000,
    .handle_page_fault = &handle_vcmu_top_fault,
    .priv = NULL
};

int
vm_install_vcmu_top(vm_t* vm)
{
    struct cmu_top_priv *cmu_top_data;
    struct device d;
    vspace_t* vmm_vspace;
    int err;

    d = dev_cmu_top;
    vmm_vspace = vm->vmm_vspace;

    /* Initialise the virtual device */
    cmu_top_data = malloc(sizeof(struct cmu_top_priv));
    if (cmu_top_data == NULL) {
        assert(cmu_top_data);
        return -1;
    }
    memset(cmu_top_data, 0, sizeof(*cmu_top_data));
    cmu_top_data->vm = vm;

    cmu_top_data->regs = map_device(vmm_vspace, vm->vka, vm->simple,
                                    d.pstart, 0, seL4_AllRights);
    if (cmu_top_data->regs == NULL) {
        return -1;
    }

    d.priv = cmu_top_data;
    err = vm_add_device(vm, &d);
    assert(!err);
    if (err) {
        free(cmu_top_data);
        return -1;
    }

    return 0;
}

