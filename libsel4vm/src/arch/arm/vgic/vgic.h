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

#include "vdist.h"
#include "rdist.h"

#pragma once

#define MAX_LR_OVERFLOW 64
#define MAX_VIRQS   200
#define NUM_SGI_VIRQS   16
#define NUM_PPI_VIRQS   16
#define GIC_SGI_IRQ_MIN     16
#define GIC_SPI_IRQ_MIN     NUM_SGI_VIRQS + NUM_PPI_VIRQS


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

    struct vgic_dist_map distributor;
    struct vgic_rdist_map redistributor;
    struct vgic_rdist_sgi_ppi_map redistributor_sgi_ppi;

    uintptr_t dist_paddr, rdist_paddr, rdist_sgi_ppi_paddr;
};

int vm_install_vgic_v2(vm_t *vm, struct vm_irq_controller_params *params);
int vm_install_vgic_v3(vm_t *vm, struct vm_irq_controller_params *params);
int vm_install_vgic(vm_t *vm, struct vm_irq_controller_params *params);

int vm_register_irq(vm_vcpu_t *vcpu, int irq, irq_ack_fn_t ack_fn, void *cookie);

int vm_v2_inject_irq(vm_vcpu_t *vcpu, int irq);
int vm_v3_inject_irq(vm_vcpu_t *vcpu, int irq);
int vm_inject_irq(vm_vcpu_t *vcpu, int irq);

int handle_vgic_v2_maintenance(vm_vcpu_t *vcpu, int idx);
int handle_vgic_v3_maintenance(vm_vcpu_t *vcpu, int idx);
int vm_vgic_maintenance_handler(vm_vcpu_t *vcpu);

void vgic_dist_v2_reset(struct vgic_dist_map *gic_dist);
void vgic_dist_v3_reset(struct vgic_dist_map *gic_dist);
void vgic_dist_reset(struct vgic_device *vgic);

void vgic_rdist_reset(struct vgic_rdist_map *gic_rdist);
void vgic_rdist_sgi_ppi_reset(struct vgic_rdist_sgi_ppi_map *gic_rdist_sgi_ppi);

memory_fault_result_t handle_vgic_dist_fault(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr,
                                             size_t fault_length,
                                             void *cookie);
memory_fault_result_t handle_vgic_rdist_fault(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr,
                                              size_t fault_length,
                                              void *cookie);
memory_fault_result_t handle_vgic_rdist_sgi_ppi_fault(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr,
                                                      size_t fault_length,
                                                      void *cookie);

int vgic_dist_enable(struct vgic_device *vgic, vm_t *vm);
int vgic_dist_disable(struct vgic_device *vgic, vm_t *vm);
int vgic_dist_enable_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq);
int vgic_dist_disable_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq);
int vgic_dist_set_pending_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq);
int vgic_dist_clr_pending_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq);

int vgic_sgi_set_pending_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq);
