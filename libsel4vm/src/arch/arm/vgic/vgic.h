/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_irq_controller.h>

#include "vgicv2_defs.h"
/* #include "vgicv3_defs.h" */

#pragma once

#define MAX_LR_OVERFLOW 64
#define MAX_VIRQS   200
#define NUM_SGI_VIRQS   16
#define NUM_PPI_VIRQS   16
#define GIC_SPI_IRQ_MIN      NUM_SGI_VIRQS + NUM_PPI_VIRQS


struct virq_handle {
    int virq;
    irq_ack_fn_t ack;
    void *token;
};

struct lr_of {
    struct virq_handle irqs[MAX_LR_OVERFLOW]; /* circular buffer */
    size_t head;
    size_t tail;
    bool full;
};

struct vgic_device {
    enum vm_gic_version version;

    /* Mirrors the vcpu list registers */
    struct virq_handle *irq[CONFIG_MAX_NUM_NODES][MAX_LR_OVERFLOW - 1];
    /* IRQs that would not fit in the vcpu list registers */
    struct lr_of lr_overflow[CONFIG_MAX_NUM_NODES];
    /* Complete set of virtual irqs */
    struct virq_handle *sgi_ppi_irq[CONFIG_MAX_NUM_NODES][NUM_SGI_VIRQS + NUM_PPI_VIRQS];
    struct virq_handle *virqs[MAX_VIRQS];

    /* Virtual distributer registers */
    union {
        struct vgic_v2_registers reg_v2;
        /* struct vgic_v3_registers reg_v3; */
    };
};

int vm_install_vgic_v2(vm_t *vm, struct vm_irq_controller_params *params);
int vm_install_vgic_v3(vm_t *vm, struct vm_irq_controller_params *params);
int vm_vgic_maintenance_handler(vm_vcpu_t *vcpu);
