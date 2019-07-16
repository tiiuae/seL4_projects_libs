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

#include <sel4arm-vmm/vm.h>
#include <sel4arm-vmm/devices.h>
#include <sel4arm-vmm/devices/generic_forward.h>

struct gf_device_priv {
    struct generic_forward_cfg cfg;
};

static int handle_gf_fault(struct device *d, vm_t *vm, fault_t *fault)
{
    struct gf_device_priv *gf_device_priv = (struct gf_device_priv *)d->priv;

    /* Gather fault information */
    uint32_t offset = fault_get_address(fault) - d->pstart;
    if (offset >= d->size) {
        ZF_LOGF("Fault on address not supported by this handler");
    }

    /* Dispatch to external fault handler */
    if (fault_is_read(fault)) {
        if (gf_device_priv->cfg.read_fn == NULL) {
            ZF_LOGD("No read function provided");
            fault_set_data(fault, 0);
        } else {
            fault_set_data(fault, gf_device_priv->cfg.read_fn(offset));
        }
    } else  {
        if (gf_device_priv->cfg.write_fn == NULL) {
            ZF_LOGD("No write function provided");
        } else {
            gf_device_priv->cfg.write_fn(offset, fault_get_data(fault));
        }
    }

    return advance_fault(fault);
}


int vm_install_generic_forward_device(vm_t *vm, const struct device *d,
                                      struct generic_forward_cfg cfg)
{
    struct gf_device_priv *gf_device_priv;
    struct device dev;
    int err;

    /* initialise private data */
    gf_device_priv = (struct gf_device_priv *)malloc(sizeof(*gf_device_priv));
    if (gf_device_priv == NULL) {
        ZF_LOGE("error malloc returned null");
        return -1;
    }
    gf_device_priv->cfg = cfg;

    /* Add the device */
    dev = *d;
    dev.priv = gf_device_priv;
    dev.handle_page_fault = &handle_gf_fault;
    err = vm_add_device(vm, &dev);
    if (err) {
        ZF_LOGE("vm_add_device returned error: %d", err);
        free(gf_device_priv);
        return -1;
    }

    return 0;
}
