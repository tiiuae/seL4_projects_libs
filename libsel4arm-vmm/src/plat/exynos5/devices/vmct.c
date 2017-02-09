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

#include <sel4arm-vmm/plat/devices.h>
#include "../../../vm.h"

//#define DEBUG_MCT
#ifdef DEBUG_MCT
#define DMCT(...) printf("MCT: " __VA_ARGS__)
#else
#define DMCT(...) do{}while(0)
#endif

#define GWSTAT_TCON          (1U << 16)
#define GWSTAT_COMP3_ADD_INC (1U << 14)
#define GWSTAT_COMP3H        (1U << 13)
#define GWSTAT_COMP3L        (1U << 12)
#define GWSTAT_COMP2_ADD_INC (1U << 10)
#define GWSTAT_COMP2H        (1U << 9)
#define GWSTAT_COMP2L        (1U << 8)
#define GWSTAT_COMP1_ADD_INC (1U << 6)
#define GWSTAT_COMP1H        (1U << 5)
#define GWSTAT_COMP1L        (1U << 4)
#define GWSTAT_COMP0_ADD_INC (1U << 2)
#define GWSTAT_COMP0H        (1U << 1)
#define GWSTAT_COMP0L        (1U << 0)


struct vmct_priv {
    uint32_t wstat;
    uint32_t cnt_wstat;
    uint32_t lwstat[4];
};

static inline struct vmct_priv* vmct_get_priv(void* priv) {
    assert(priv);
    return (struct vmct_priv*)priv;
}


static int
handle_vmct_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    struct vmct_priv* mct_priv;
    int offset;
    uint32_t mask;

    mct_priv = vmct_get_priv(d->priv);

    /* Gather fault information */
    offset = fault_get_address(fault) - d->pstart;
    mask = fault_get_data_mask(fault);
    /* Handle the fault */
    if (offset < 0x300) {
        /*** Global ***/
        uint32_t *wstat;
        uint32_t *cnt_wstat;
        wstat = &mct_priv->wstat;
        cnt_wstat = &mct_priv->cnt_wstat;
        if (fault_is_write(fault)) {
            if (offset >= 0x100 && offset < 0x108) {
                /* Count registers */
                *cnt_wstat |= (1 << (offset - 0x100) / 4);
            } else if (offset == 0x110) {
                *cnt_wstat &= ~(fault_get_data(fault) & mask);
            } else if (offset >= 0x200 && offset < 0x244) {
                /* compare registers */
                *wstat |= (1 << (offset - 0x200) / 4);
            } else if (offset == 0x24C) {
                /* Write status */
                *wstat &= ~(fault_get_data(fault) & mask);
            } else {
                DMCT("global MCT fault on unknown offset 0x%x\n", offset);
            }
        } else {
            /* read fault */
            if (offset == 0x110) {
                fault_set_data(fault, *cnt_wstat);
            } else if (offset == 0x24C) {
                fault_set_data(fault, *wstat);
            } else {
                fault_set_data(fault, 0);
            }
        }
    } else {
        /*** Local ***/
        int timer = (offset - 0x300) / 0x100;
        int loffset = (offset - 0x300) - (timer * 0x100);
        uint32_t *wstat = &mct_priv->lwstat[timer];
        if (fault_is_write(fault)) {
            if (loffset == 0x0) {        /* tcompl */
                *wstat |= (1 << 1);
            } else if (loffset == 0x8) {  /* tcomph */
                *wstat |= (1 << 1);
            } else if (loffset == 0x20) { /* tcon */
                *wstat |= (1 << 3);
            } else if (loffset == 0x34) { /* int_en */
                /* Do nothing */
            } else if (loffset == 0x40) { /* wstat */
                *wstat &= ~(fault_get_data(fault) & mask);
            } else {
                DMCT("local MCT fault on unknown offset 0x%x\n", offset);
            }
        } else {
            /* read fault */
            if (loffset == 0x40) {
                fault_set_data(fault, *wstat);
            } else {
                fault_set_data(fault, 0);
            }
        }
    }
    return advance_fault(fault);
}

const struct device dev_vmct_timer = {
    .devid = DEV_MCT_TIMER,
    .name = "mct",

    .pstart = MCT_ADDR,

    .size = 0x1000,
    .handle_page_fault = handle_vmct_fault,
    .priv = NULL
};


int vm_install_vmct(vm_t* vm)
{
    struct vmct_priv *vmct_data;
    struct device d;
    int err;
    d = dev_vmct_timer;
    /* Initialise the virtual device */
    vmct_data = malloc(sizeof(struct vmct_priv));
    if (vmct_data == NULL) {
        assert(vmct_data);
        return -1;
    }
    memset(vmct_data, 0, sizeof(*vmct_data));

    d.priv = vmct_data;
    err = vm_add_device(vm, &d);
    assert(!err);
    if (err) {
        free(vmct_data);
        return -1;
    }
    return 0;
}
