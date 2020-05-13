#pragma once

/* GIC Distributor register access utilities */
#define GIC_DIST_REGN(offset, reg) ((offset-reg)/sizeof(uint32_t))
#define RANGE32(a, b) a ... b + (sizeof(uint32_t)-1)

#define IRQ_IDX(irq) ((irq) / 32)
#define IRQ_BIT(irq) (1U << ((irq) % 32))

#define not_pending(...) !is_pending(__VA_ARGS__)
#define not_active(...)  !is_active(__VA_ARGS__)
#define not_enabled(...) !is_enabled(__VA_ARGS__)

#ifdef GIC_V2
#define GIC_DIST_TYPE struct vgic_v2_dist_map
#else
#define GIC_DIST_TYPE struct vgic_v3_dist_map
#endif

static inline void set_sgi_ppi_pending(GIC_DIST_TYPE *gic_dist, int irq, int value, int vcpu_id)
{
    if (value) {
        gic_dist->pending_set0[vcpu_id] |= IRQ_BIT(irq);
        gic_dist->pending_clr0[vcpu_id] |= IRQ_BIT(irq);
    } else {
        gic_dist->pending_set0[vcpu_id] &= ~IRQ_BIT(irq);
        gic_dist->pending_clr0[vcpu_id] &= ~IRQ_BIT(irq);
    }
}

static inline void set_spi_pending(GIC_DIST_TYPE *gic_dist, int irq, int value, int vcpu_id)
{
    if (value) {
        gic_dist->pending_set[IRQ_IDX(irq)] |= IRQ_BIT(irq);
        gic_dist->pending_clr[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->pending_set[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
        gic_dist->pending_clr[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline void set_pending(GIC_DIST_TYPE *gic_dist, int irq, int value, int vcpu_id)
{
    if (irq < GIC_SPI_IRQ_MIN) {
        set_sgi_ppi_pending(gic_dist, irq, value, vcpu_id);
        return;
    }
    set_spi_pending(gic_dist, irq, value, vcpu_id);
}

static inline int is_sgi_ppi_pending(GIC_DIST_TYPE *gic_dist, int irq, int vcpu_id)
{
    return !!(gic_dist->pending_set0[vcpu_id] & IRQ_BIT(irq));
}

static inline int is_spi_pending(GIC_DIST_TYPE *gic_dist, int irq, int vcpu_id)
{
    return !!(gic_dist->pending_set[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static inline int is_pending(GIC_DIST_TYPE *gic_dist, int irq, int vcpu_id)
{
    if (irq < GIC_SPI_IRQ_MIN) {
        return is_sgi_ppi_pending(gic_dist, irq, vcpu_id);

    }
    return is_spi_pending(gic_dist, irq, vcpu_id);
}

static inline void set_sgi_ppi_enable(GIC_DIST_TYPE *gic_dist, int irq, int value, int vcpu_id)
{
    if (value) {
        gic_dist->enable_set0[vcpu_id] |= IRQ_BIT(irq);
        gic_dist->enable_clr0[vcpu_id] |= IRQ_BIT(irq);
    } else {
        gic_dist->enable_set0[vcpu_id] &= ~IRQ_BIT(irq);
        gic_dist->enable_clr0[vcpu_id] &= ~IRQ_BIT(irq);
    }
}


static inline void set_spi_enable(GIC_DIST_TYPE *gic_dist, int irq, int value, int vcpu_id)
{
    if (value) {
        gic_dist->enable_set[IRQ_IDX(irq)] |= IRQ_BIT(irq);
        gic_dist->enable_clr[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->enable_set[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
        gic_dist->enable_clr[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline void set_enable(GIC_DIST_TYPE *gic_dist, int irq, int value, int vcpu_id)
{
    if (irq < GIC_SPI_IRQ_MIN) {
        set_sgi_ppi_enable(gic_dist, irq, value, vcpu_id);
        return;
    }
    set_spi_enable(gic_dist, irq, value, vcpu_id);
}

static inline int is_sgi_ppi_enabled(GIC_DIST_TYPE *gic_dist, int irq, int vcpu_id)
{
    return !!(gic_dist->enable_set0[vcpu_id] & IRQ_BIT(irq));
}

static inline int is_spi_enabled(GIC_DIST_TYPE *gic_dist, int irq, int vcpu_id)
{
    return !!(gic_dist->enable_set[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static inline int is_enabled(GIC_DIST_TYPE *gic_dist, int irq, int vcpu_id)
{
    if (irq < GIC_SPI_IRQ_MIN) {
        return is_sgi_ppi_enabled(gic_dist, irq, vcpu_id);
    }
    return is_spi_enabled(gic_dist, irq, vcpu_id);
}

static inline void set_sgi_ppi_active(GIC_DIST_TYPE *gic_dist, int irq, int value, int vcpu_id)
{
    if (value) {
        gic_dist->active0[vcpu_id] |= IRQ_BIT(irq);
    } else {
        gic_dist->active0[vcpu_id] &= ~IRQ_BIT(irq);
    }
}

static inline void set_spi_active(GIC_DIST_TYPE *gic_dist, int irq, int value, int vcpu_id)
{
    if (value) {
        gic_dist->active[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->active[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline void set_active(GIC_DIST_TYPE *gic_dist, int irq, int value, int vcpu_id)
{
    if (irq < GIC_SPI_IRQ_MIN) {
        set_sgi_ppi_active(gic_dist, irq, value, vcpu_id);
        return;
    }
    set_spi_active(gic_dist, irq, value, vcpu_id);
}

static inline int is_sgi_ppi_active(GIC_DIST_TYPE *gic_dist, int irq, int vcpu_id)
{
    return !!(gic_dist->active0[vcpu_id] & IRQ_BIT(irq));
}

static inline int is_spi_active(GIC_DIST_TYPE *gic_dist, int irq, int vcpu_id)
{
    return !!(gic_dist->active[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static inline int is_active(GIC_DIST_TYPE *gic_dist, int irq, int vcpu_id)
{
    if (irq < GIC_SPI_IRQ_MIN) {
        return is_sgi_ppi_active(gic_dist, irq, vcpu_id);
    }
    return is_spi_active(gic_dist, irq, vcpu_id);
}
