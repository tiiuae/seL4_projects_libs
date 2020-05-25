#include "virq.h"

void virq_ack(vm_vcpu_t *vcpu, struct virq_handle *irq)
{
    irq->ack(vcpu, irq->virq, irq->token);
}

struct virq_handle *virq_get_sgi_ppi(struct vgic_device *vgic, vm_vcpu_t *vcpu, int virq)
{
    assert(vcpu->vcpu_id < CONFIG_MAX_NUM_NODES);
    return vgic->sgi_ppi_irq[vcpu->vcpu_id][virq];
}

struct virq_handle *virq_find_spi_irq_data(struct vgic_device *vgic, int virq)
{
    int i;
    for (i = 0; i < MAX_VIRQS; i++) {
        if (vgic->virqs[i] && vgic->virqs[i]->virq == virq) {
            return vgic->virqs[i];
        }
    }
    return NULL;
}

struct virq_handle *virq_find_irq_data(struct vgic_device *vgic, vm_vcpu_t *vcpu, int virq)
{
    if (virq < GIC_SPI_IRQ_MIN)  {
        return virq_get_sgi_ppi(vgic, vcpu, virq);
    }
    return virq_find_spi_irq_data(vgic, virq);
}

int virq_spi_add(struct vgic_device *vgic, struct virq_handle *virq_data)
{
    int i;
    for (i = 0; i < MAX_VIRQS; i++) {
        if (vgic->virqs[i] == NULL) {
            vgic->virqs[i] = virq_data;
            return 0;
        }
    }
    return -1;
}

int virq_sgi_ppi_add(vm_vcpu_t *vcpu, struct vgic_device *vgic, struct virq_handle *virq_data)
{
    if (vgic->sgi_ppi_irq[vcpu->vcpu_id][virq_data->virq] != NULL) {
        ZF_LOGE("VIRQ %d already registered for VCPU %u\n", virq_data->virq, vcpu->vcpu_id);
        return -1;
    }
    vgic->sgi_ppi_irq[vcpu->vcpu_id][virq_data->virq] = virq_data;
    return 0;
}

int virq_add(vm_vcpu_t *vcpu, struct vgic_device *vgic, struct virq_handle *virq_data)
{
    int virq = virq_data->virq;
    if (virq < GIC_SPI_IRQ_MIN) {
        return virq_sgi_ppi_add(vcpu, vgic, virq_data);
    }
    return virq_spi_add(vgic, virq_data);
}

void vgic_virq_init(struct vgic_device *vgic)
{
    memset(vgic->virqs, 0, sizeof(vgic->virqs));
    for (int i = 0; i < CONFIG_MAX_NUM_NODES; i++) {
        memset(vgic->irq[i], 0, sizeof(vgic->irq[i]));
        vgic->lr_overflow[i].head = 0;
        vgic->lr_overflow[i].tail = 0;
        vgic->lr_overflow[i].full = false;
        memset(vgic->lr_overflow[i].irqs, 0, sizeof(vgic->lr_overflow[i].irqs));
    }
}

void virq_init(struct virq_handle *virq, int irq, irq_ack_fn_t ack_fn, void *token)
{
    virq->virq = irq;
    virq->token = token;
    virq->ack = ack_fn;
}

int vgic_add_overflow_cpu(struct lr_of *lr_overflow, struct virq_handle *irq)
{
    /* Add to overflow list */
    int idx = lr_overflow->tail;
    if (unlikely(lr_overflow->full)) {
        ZF_LOGF("too many overflow irqs");
        return -1;
    }

    lr_overflow->irqs[idx] = *irq;
    lr_overflow->full = (lr_overflow->head == LR_OF_NEXT(lr_overflow->tail));
    if (!lr_overflow->full) {
        lr_overflow->tail = LR_OF_NEXT(idx);
    }
    return 0;
}

int vgic_add_overflow(struct vgic_device *vgic, struct virq_handle *irq, vm_vcpu_t *vcpu)
{
    return vgic_add_overflow_cpu(&vgic->lr_overflow[vcpu->vcpu_id], irq);
}

/* Inject interrupt into vcpu */
int vgic_vcpu_inject_irq(struct vgic_device *vgic, vm_vcpu_t *inject_vcpu, struct virq_handle *irq)
{
    int err;
    int i;

    seL4_CPtr vcpu;
    vcpu = inject_vcpu->vcpu.cptr;
    for (i = 0; i < MAX_LR_OVERFLOW; i++) {
        if (vgic->irq[inject_vcpu->vcpu_id][i] == NULL) {
            break;
        }
    }

    switch (vgic->version) {
        case VM_GIC_V2:
            err = seL4_ARM_VCPU_InjectIRQ(vcpu, irq->virq, 0, 0, i);
            break;
        case VM_GIC_V3:
            err = seL4_ARM_VCPU_InjectIRQ(vcpu, irq->virq, 0, 1, i);
            break;
        default:
            return -1;
    }

    assert((i < 4) || err);
    if (!err) {
        /* Shadow */
        vgic->irq[inject_vcpu->vcpu_id][i] = irq;
        return err;
    } else {
        /* Add to overflow list */
        return vgic_add_overflow(vgic, irq, inject_vcpu);
    }
}

void vgic_handle_overflow_cpu(struct vgic_device *vgic, struct lr_of *lr_overflow, vm_vcpu_t *vcpu)
{
    /* copy tail, as vgic_vcpu_inject_irq can mutate it, and we do
     * not want to process any new overflow irqs */
    size_t tail = lr_overflow->tail;
    for (size_t i = lr_overflow->head; i != tail; i = LR_OF_NEXT(i)) {
        if (vgic_vcpu_inject_irq(vgic, vcpu, &lr_overflow->irqs[i]) == 0) {
            lr_overflow->head = LR_OF_NEXT(i);
            lr_overflow->full = (lr_overflow->head == LR_OF_NEXT(lr_overflow->tail));
        } else {
            break;
        }
    }
}

void vgic_handle_overflow(struct vgic_device *vgic, vm_vcpu_t *vcpu)
{
    vgic_handle_overflow_cpu(vgic, &vgic->lr_overflow[vcpu->vcpu_id], vcpu);
}
