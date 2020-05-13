#pragma once

#include "virq.h"
#include "vgic_common.h"

#define _REG_V(v) reg_v##v
#define _GET_VGIC_DIST(vgic, v) struct vgic_v##v##_dist_map *gic_dist = &(vgic)->_REG_V(v).distributor

#ifdef GIC_V2
#define GET_VGIC_DIST(vgic) _GET_VGIC_DIST(vgic, 2)
#else
#define GET_VGIC_DIST(vgic) _GET_VGIC_DIST(vgic, 3)
#endif

#ifdef DEBUG_DIST
#define DDIST(...) do{ printf("VDIST: "); printf(__VA_ARGS__); }while(0)
#else
#define DDIST(...) do{}while(0)
#endif

static int vgic_dist_enable(struct vgic_device *vgic, vm_t *vm)
{
    GET_VGIC_DIST(vgic);
    DDIST("enabling gic distributer\n");
    gic_dist->enable = 1;
    return 0;
}

static int vgic_dist_disable(struct vgic_device *vgic, vm_t *vm)
{
    GET_VGIC_DIST(vgic);
    DDIST("disabling gic distributer\n");
    gic_dist->enable = 0;
    return 0;
}

static int vgic_dist_enable_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq)
{
    GET_VGIC_DIST(vgic);
    struct virq_handle *virq_data;
    DDIST("enabling irq %d\n", irq);
    set_enable(gic_dist, irq, true, vcpu->vcpu_id);
    virq_data = virq_find_irq_data(vgic, vcpu, irq);
    if (virq_data) {
        /* STATE b) */
        if (not_pending(gic_dist, virq_data->virq, vcpu->vcpu_id)) {
            virq_ack(vcpu, virq_data);
        }
    } else {
        DDIST("enabled irq %d has no handle", irq);
    }
    return 0;
}

static int vgic_dist_disable_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq)
{
    /* STATE g) */
    GET_VGIC_DIST(vgic);
    if (irq >= 16) {
        DDIST("disabling irq %d\n", irq);
        set_enable(gic_dist, irq, false, vcpu->vcpu_id);
    }
    return 0;
}

static int vgic_dist_set_pending_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq)
{
    /* STATE c) */
    GET_VGIC_DIST(vgic);
    struct virq_handle *virq_data;

    virq_data = virq_find_irq_data(vgic, vcpu, irq);
    /* If it is enables, inject the IRQ */
    if (virq_data && gic_dist->enable && is_enabled(gic_dist, irq, vcpu->vcpu_id)) {
        int err;
        DDIST("Pending set: Inject IRQ from pending set (%d)\n", irq);

        set_pending(gic_dist, virq_data->virq, true, vcpu->vcpu_id);
        err = vgic_vcpu_inject_irq(vgic, vcpu, virq_data);
        assert(!err);

        return err;
    } else {
        /* No further action */
        DDIST("IRQ not enabled (%d) on vcpu %d\n", irq, vcpu->vcpu_id);
        return -1;
    }

    return 0;
}

static int vgic_dist_clr_pending_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq)
{
    GET_VGIC_DIST(vgic);
    DDIST("clr pending irq %d\n", irq);
    set_pending(gic_dist, irq, false, vcpu->vcpu_id);
    return 0;
}
