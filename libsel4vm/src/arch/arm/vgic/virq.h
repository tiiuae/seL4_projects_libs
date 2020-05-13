#pragma once

#include "vgic.h"

#define LR_OF_NEXT(_i) (((_i) == MAX_LR_OVERFLOW - 1) ? 0 : ((_i) + 1))

void virq_ack(vm_vcpu_t *vcpu, struct virq_handle *irq);

struct virq_handle *virq_get_sgi_ppi(struct vgic_device *vgic, vm_vcpu_t *vcpu, int virq);
struct virq_handle *virq_find_spi_irq_data(struct vgic_device *vgic, int virq);
struct virq_handle *virq_find_irq_data(struct vgic_device *vgic, vm_vcpu_t *vcpu, int virq);

int virq_spi_add(struct vgic_device *vgic, struct virq_handle *virq_data);
int virq_sgi_ppi_add(vm_vcpu_t *vcpu, struct vgic_device *vgic, struct virq_handle *virq_data);
int virq_add(vm_vcpu_t *vcpu, struct vgic_device *vgic, struct virq_handle *virq_data);

void vgic_virq_init(struct vgic_device *vgic);
void virq_init(struct virq_handle *virq, int irq, irq_ack_fn_t ack_fn, void *token);

int vgic_add_overflow_cpu(struct lr_of *lr_overflow, struct virq_handle *irq);
int vgic_add_overflow(struct vgic_device *vgic, struct virq_handle *irq, vm_vcpu_t *vcpu);
void vgic_handle_overflow_cpu(struct vgic_device *vgic, struct lr_of *lr_overflow, vm_vcpu_t *vcpu);
void vgic_handle_overflow(struct vgic_device *vgic, vm_vcpu_t *vcpu);

int vgic_vcpu_inject_irq(struct vgic_device *vgic, vm_vcpu_t *inject_vcpu, struct virq_handle *irq);
