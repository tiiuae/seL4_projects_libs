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

#define EMMC_DBADDR        0x88
#define EMMC_DSCADDR       0x94
#define EMMC_BUFADDR       0x98

//#define SDHC_DS_ADDR       0x00
//#define SDHC_ADMA_SYS_ADDR 0x58

struct sdhc_priv {
    struct dma *dma_list;
    vm_t *vm;
    void* regs;
};

static int
handle_sdhc_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    struct sdhc_priv* sdhc_data = (struct sdhc_priv*)d->priv;
    volatile uint32_t *reg;
    int offset;

    /* Gather fault information */
    offset = fault->addr - d->pstart;
    reg = (uint32_t*)(sdhc_data->regs + offset);
    /* Handle the fault */
    reg = (volatile uint32_t*)(sdhc_data->regs + offset);
    if (fault_is_read(fault)) {
        fault->data = *reg;
        if (offset != 0x44)
            printf("[%s] pc0x%x| r0x%x:0x%x\n", d->name,
                   fault->regs.pc,
                   fault->addr,
                   fault->data);
#if 0
    } else if(offset == DSCADDR || offset == BUFADDR || offset == SDMMC_DBADDR){
        printf("[%s] access violation pc0x%x| w0x%x:0x%x\n", d->name,
               fault->regs.pc,
               fault->addr,
               fault->data);
#endif
    } else {
        printf("[%s] pc0x%x| w0x%x:0x%x\n", d->name,
               fault->regs.pc,
               fault->addr,
               fault->data);
        *reg = fault_emulate(fault, *reg);
    }
    return advance_fault(fault);
}


const struct device dev_msh0 = {
    .devid = DEV_CUSTOM,
    .name = "MSH0",
    .pstart = MSH0_PADDR,
    .size = 0x1000,
    .handle_page_fault = &handle_sdhc_fault,
    .priv = NULL
};

const struct device dev_msh2 = {
    .devid = DEV_CUSTOM,
    .name = "MSH2",
    .pstart = MSH2_PADDR,
    .size = 0x1000,
    .handle_page_fault = &handle_sdhc_fault,
    .priv = NULL
};

static int
vm_install_nodma_sdhc(vm_t* vm, int idx)
{
    struct sdhc_priv *sdhc_data;
    struct device d;
    vspace_t* vmm_vspace;
    int err;
    switch (idx) {
    case 0:
        d = dev_msh0;
        break;
    case 2:
        d = dev_msh2;
        break;
    default:
        assert(0);
        return -1;
    }

    vmm_vspace = vm->vmm_vspace;

    /* Initialise the virtual device */
    sdhc_data = malloc(sizeof(struct sdhc_priv));
    if (sdhc_data == NULL) {
        assert(sdhc_data);
        return -1;
    }
    memset(sdhc_data, 0, sizeof(*sdhc_data));
    sdhc_data->vm = vm;

    sdhc_data->regs = map_device(vmm_vspace, vm->vka, vm->simple,
                                 d.pstart, 0, seL4_AllRights);
    if (sdhc_data->regs == NULL) {
        assert(sdhc_data->regs);
        return -1;
    }
    d.priv = sdhc_data;
    err = vm_add_device(vm, &d);
    assert(!err);
    if (err) {
        free(sdhc_data);
        return -1;
    }
    return 0;
}

int vm_install_nodma_sdhc0(vm_t* vm)
{
    return vm_install_nodma_sdhc(vm, 0);
}

int vm_install_nodma_sdhc2(vm_t* vm)
{
    return vm_install_nodma_sdhc(vm, 2);
}
