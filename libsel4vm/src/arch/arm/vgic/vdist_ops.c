#include "vgic.h"
#include "virq.h"

#define IRQ_IDX(irq) ((irq) / 32)
#define IRQ_BIT(irq) (1U << ((irq) % 32))

#define not_pending(...) !is_pending(__VA_ARGS__)
#define not_active(...)  !is_active(__VA_ARGS__)
#define not_enabled(...) !is_enabled(__VA_ARGS__)

static inline void set_sgi_ppi_pending(struct vgic_dist_map *gic_dist, int irq, int value, int vcpu_id)
{
    if (value) {
        gic_dist->pending_set0[vcpu_id] |= IRQ_BIT(irq);
        gic_dist->pending_clr0[vcpu_id] |= IRQ_BIT(irq);
    } else {
        gic_dist->pending_set0[vcpu_id] &= ~IRQ_BIT(irq);
        gic_dist->pending_clr0[vcpu_id] &= ~IRQ_BIT(irq);
    }
}

static inline void set_spi_pending(struct vgic_dist_map *gic_dist, int irq, int value, int vcpu_id)
{
    if (value) {
        gic_dist->pending_set[IRQ_IDX(irq)] |= IRQ_BIT(irq);
        gic_dist->pending_clr[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->pending_set[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
        gic_dist->pending_clr[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline void set_pending(struct vgic_dist_map *gic_dist, int irq, int value, int vcpu_id)
{
    if (irq < GIC_SPI_IRQ_MIN) {
        set_sgi_ppi_pending(gic_dist, irq, value, vcpu_id);
        return;
    }
    set_spi_pending(gic_dist, irq, value, vcpu_id);
}

static inline int is_sgi_ppi_pending(struct vgic_dist_map *gic_dist, int irq, int vcpu_id)
{
    return !!(gic_dist->pending_set0[vcpu_id] & IRQ_BIT(irq));
}

static inline int is_spi_pending(struct vgic_dist_map *gic_dist, int irq, int vcpu_id)
{
    return !!(gic_dist->pending_set[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static inline int is_pending(struct vgic_dist_map *gic_dist, int irq, int vcpu_id)
{
    if (irq < GIC_SPI_IRQ_MIN) {
        return is_sgi_ppi_pending(gic_dist, irq, vcpu_id);

    }
    return is_spi_pending(gic_dist, irq, vcpu_id);
}

static inline void set_sgi_ppi_enable(struct vgic_dist_map *gic_dist, int irq, int value, int vcpu_id)
{
    if (value) {
        gic_dist->enable_set0[vcpu_id] |= IRQ_BIT(irq);
        gic_dist->enable_clr0[vcpu_id] |= IRQ_BIT(irq);
    } else {
        gic_dist->enable_set0[vcpu_id] &= ~IRQ_BIT(irq);
        gic_dist->enable_clr0[vcpu_id] &= ~IRQ_BIT(irq);
    }
}

static inline void set_spi_enable(struct vgic_dist_map *gic_dist, int irq, int value, int vcpu_id)
{
    if (value) {
        gic_dist->enable_set[IRQ_IDX(irq)] |= IRQ_BIT(irq);
        gic_dist->enable_clr[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->enable_set[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
        gic_dist->enable_clr[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline void set_enable(struct vgic_dist_map *gic_dist, int irq, int value, int vcpu_id)
{
    if (irq < GIC_SPI_IRQ_MIN) {
        set_sgi_ppi_enable(gic_dist, irq, value, vcpu_id);
        return;
    }
    set_spi_enable(gic_dist, irq, value, vcpu_id);
}

static inline int is_sgi_ppi_enabled(struct vgic_dist_map *gic_dist, int irq, int vcpu_id)
{
    return !!(gic_dist->enable_set0[vcpu_id] & IRQ_BIT(irq));
}

static inline int is_spi_enabled(struct vgic_dist_map *gic_dist, int irq, int vcpu_id)
{
    return !!(gic_dist->enable_set[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static inline int is_enabled(struct vgic_dist_map *gic_dist, int irq, int vcpu_id)
{
    if (irq < GIC_SPI_IRQ_MIN) {
        return is_sgi_ppi_enabled(gic_dist, irq, vcpu_id);
    }
    return is_spi_enabled(gic_dist, irq, vcpu_id);
}

int vgic_dist_enable(struct vgic_device *vgic, vm_t *vm)
{
    struct vgic_dist_map *gic_dist = &vgic->distributor;
    DDIST("enabling gic distributer\n");
    gic_dist->enable = 1;
    return 0;
}

int vgic_dist_disable(struct vgic_device *vgic, vm_t *vm)
{
    struct vgic_dist_map *gic_dist = &vgic->distributor;
    DDIST("disabling gic distributer\n");
    gic_dist->enable = 0;
    return 0;
}

int vgic_dist_enable_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq)
{
    struct vgic_dist_map *gic_dist = &vgic->distributor;
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

int vgic_dist_disable_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq)
{
    /* STATE g) */
    struct vgic_dist_map *gic_dist = &vgic->distributor;
    if (irq >= 16) {
        DDIST("disabling irq %d\n", irq);
        set_enable(gic_dist, irq, false, vcpu->vcpu_id);
    }
    return 0;
}

int vgic_dist_set_pending_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq)
{
    /* STATE c) */
    struct vgic_dist_map *gic_dist = &vgic->distributor;
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

int vgic_dist_clr_pending_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq)
{
    struct vgic_dist_map *gic_dist = &vgic->distributor;
    DDIST("clr pending irq %d\n", irq);
    set_pending(gic_dist, irq, false, vcpu->vcpu_id);
    return 0;
}
