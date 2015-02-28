/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*
 * DesignWare eMMC
 */

#include <stdlib.h>
#include <string.h>

#include "../../devices.h"
#include "../../vm.h"

#define DWEMMC_DBADDR_OFFSET    0x088
#define DWEMMC_DSCADDR_OFFSET   0x094
#define DWEMMC_BUFADDR_OFFSET   0x098
#define DWEMMC_BLASH_OFFSET     0x200
#define DWEMMC_DATA_OFFSET      0x100
#define DWEMMC_DATA_240A_OFFSET 0x200
//#define SDHC_DS_ADDR       0x00
//#define SDHC_ADMA_SYS_ADDR 0x58


#if 0
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...) do { } while(0)
#endif

struct sdhc_priv {
/// The VM associated with this device
    vm_t *vm;
/// Physical registers of the SDHC
    void* regs;
/// Residual for 64 bit atomic access to FIFO
    uint32_t a64;
};

static int
handle_sdhc_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    struct sdhc_priv* sdhc_data = (struct sdhc_priv*)d->priv;
    volatile uint32_t *reg;
    int offset;

    /* Gather fault information */
    offset = fault_get_address(fault) - d->pstart;
    reg = (uint32_t*)(sdhc_data->regs + offset);
    /* Handle the fault */
    reg = (volatile uint32_t*)(sdhc_data->regs + offset);
    if (fault_is_read(fault)) {
        if (fault_get_width(fault) == WIDTH_DOUBLEWORD) {
            if (offset & 0x4) {
                /* Unaligned access: report residual */
                fault_set_data(fault, sdhc_data->a64);
            } else {
                /* Aligned access: Read in and store residual */
                uint64_t v;
                v = *(volatile uint64_t*)reg;
                fault_set_data(fault, v);
                sdhc_data->a64 = v >> 32;
            }
        } else {
            assert(fault_get_width(fault) == WIDTH_WORD);
            fault_set_data(fault, *reg);
        }
        dprintf("[%s] pc0x%x| r0x%x:0x%x\n", d->name, fault_get_ctx(fault)->pc,
                fault_get_address(fault), fault_get_data(fault));

#if 0
        if (offset != 0x44)
            dprintf("[%s] pc0x%x| r0x%x:0x%x\n", d->name,
                    fault->regs.pc,
                    fault->addr,
                    fault->data);
    } else if (offset == DSCADDR || offset == BUFADDR || offset == SDMMC_DBADDR) {
        printf("[%s] access violation pc0x%x| w0x%x:0x%x\n", d->name,
               fault->regs.pc,
               fault->addr,
               fault->data);
#endif
    } else {
        switch (offset) {
        case 666:
        default:
            if (fault_get_width(fault) == WIDTH_DOUBLEWORD) {
                if (offset & 0x4) {
                    /* Unaligned acces: store data and residual */
                    uint64_t v;
                    v = ((uint64_t)fault_get_data(fault) << 32) | sdhc_data->a64;
                    *(volatile uint64_t*)reg = v;
                } else {
                    /* Aligned access: record residual */
                    sdhc_data->a64 = fault_get_data(fault);
                }
            } else {
                assert(fault_get_width(fault) == WIDTH_WORD);
                *reg = fault_get_data(fault);
            }
        }

        dprintf("[%s] pc0x%x| w0x%x:0x%x\n", d->name, fault_get_ctx(fault)->pc,
                fault_get_address(fault), fault_get_data(fault));
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
    map_vm_device(vm, d.pstart, d.pstart, seL4_CanRead);

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
