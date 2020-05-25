#include "vgic.h"
#include "virq.h"
#include "../fault.h"

#define IRQ_IDX(irq) ((irq) / 32)
#define IRQ_BIT(irq) (1U << ((irq) % 32))

#define STRUCT_OFFSET(s, m)                 ((void*)&((s)->m) - (void*)(s))
#define STRUCT_RANGE(offset, s, m)          ((offset) >= STRUCT_OFFSET(s, m) && (offset) < STRUCT_OFFSET(s, m) + sizeof((s)->m))
#define IRQ_BASE(offset, dist, enabler)     (((offset) - STRUCT_OFFSET(dist, enabler)) * 8)
#define OFFSET_TO_IDX(offset, dist, reg)    (((offset) - STRUCT_OFFSET(dist, reg)) / 4)


static inline void sgi_set_pending(struct vgic_rdist_sgi_ppi_map *gic_sgi, int irq, int v)
{
    if (v) {
        gic_sgi->ispend[IRQ_IDX(irq)] |= IRQ_BIT(irq);
        gic_sgi->icpend[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_sgi->ispend[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
        gic_sgi->icpend[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline int sgi_is_pending(struct vgic_rdist_sgi_ppi_map *gic_sgi, int irq)
{
    return !!(gic_sgi->ispend[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static inline void sgi_set_enable(struct vgic_rdist_sgi_ppi_map *gic_sgi, int irq, int v)
{
    if (v) {
        gic_sgi->isenable[IRQ_IDX(irq)] |= IRQ_BIT(irq);
        gic_sgi->icenable[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_sgi->isenable[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
        gic_sgi->icenable[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline int sgi_is_enabled(struct vgic_rdist_sgi_ppi_map *gic_sgi, int irq)
{
    return !!(gic_sgi->isenable[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static int vgic_sgi_enable_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq)
{
    struct vgic_rdist_sgi_ppi_map *gic_sgi = &vgic->redistributor_sgi_ppi;

    if (irq >= GIC_SGI_IRQ_MIN) {
        DDIST("sgi enabling irq %d\n", irq);
        struct virq_handle *virq_data;

        virq_data = virq_find_irq_data(vgic, vcpu, irq);
        sgi_set_enable(gic_sgi, irq, true);
        if (virq_data) {
            /* STATE b) */
            if (!sgi_is_pending(gic_sgi, virq_data->virq)) {
                DDIST("IRQ not pending\n");
                virq_ack(vcpu, virq_data);
            }
        } else {
            DDIST("enabled irq %d has no handle\n", irq);
        }
    }
    return 0;
}

static int vgic_sgi_disable_irq(struct vgic_device *vgic, int irq)
{
    struct vgic_rdist_sgi_ppi_map *gic_sgi = &vgic->redistributor_sgi_ppi;

    if (irq >= GIC_SGI_IRQ_MIN) {
        DDIST("sgi disabling irq %d\n", irq);
        sgi_set_enable(gic_sgi, irq, false);
    }
    return 0;
}

int vgic_sgi_set_pending_irq(struct vgic_device *vgic, vm_vcpu_t *vcpu, int irq)
{
    struct vgic_dist_map *gic_dist = &vgic->distributor;
    struct vgic_rdist_sgi_ppi_map *gic_sgi = &vgic->redistributor_sgi_ppi;
    struct virq_handle *virq_data;

    virq_data = virq_find_irq_data(vgic, vcpu, irq);

    /* If it is enables, inject the IRQ */
    if (virq_data && gic_dist->enable && sgi_is_enabled(gic_sgi, irq)) {
        int err;
        DDIST("Pending set: Inject IRQ from pending set (%d)\n", irq);

        sgi_set_pending(gic_sgi, virq_data->virq, true);
        err = vgic_vcpu_inject_irq(vgic, vcpu, virq_data);
        assert(!err);

        return err;
    } else {
        /* No further action */
        DDIST("IRQ not enabled (%d) for %s\n", irq, vcpu->vm->name);
    }
    return 0;
}

static enum gic_dist_action vgic_rdist_get_action(struct vgic_rdist_map *rdist, uintptr_t offset) {
    return ACTION_READONLY;
}

static enum gic_dist_action vgic_sgi_get_action(struct vgic_rdist_sgi_ppi_map *rdist, uintptr_t offset)
{
    if (STRUCT_RANGE(offset, rdist, isenable)) {
        return ACTION_ENABLE_SET;
    } else if (STRUCT_RANGE(offset, rdist, icenable)) {
        return ACTION_ENABLE_CLR;
    } else {
        return ACTION_READONLY;
    }
}

void vgic_rdist_reset(struct vgic_rdist_map *gic_rdist)
{
    memset(gic_rdist, 0, sizeof(*gic_rdist));

    gic_rdist->typer           = 0x1;      /* RO */
    gic_rdist->iidr            = 0x1143B;  /* RO */

    gic_rdist->pidr0           = 0x93;     /* RO */
    gic_rdist->pidr1           = 0xB4;     /* RO */
    gic_rdist->pidr2           = 0x3B;     /* RO */
    gic_rdist->pidr4           = 0x44;     /* RO */

    gic_rdist->cidr0           = 0x0D;     /* RO */
    gic_rdist->cidr1           = 0xF0;     /* RO */
    gic_rdist->cidr2           = 0x05;     /* RO */
    gic_rdist->cidr3           = 0xB1;     /* RO */
}

void vgic_rdist_sgi_ppi_reset(struct vgic_rdist_sgi_ppi_map *gic_sgi)
{
    memset(gic_sgi, 0, sizeof(*gic_sgi));
    gic_sgi->isactive[0]       = 0xaaaaaaaa;
}

memory_fault_result_t handle_vgic_rdist_fault(vm_t *vm,
                                              vm_vcpu_t *vcpu,
                                              uintptr_t fault_addr,
                                              size_t fault_length,
                                              void* cookie)
{
    enum gic_dist_action action;
    fault_t *fault = vcpu->vcpu_arch.fault;
    struct vgic_device *vgic = vm->arch.vgic;
    struct vgic_rdist_map *gic_rdist = &vgic->redistributor;
    unsigned int offset;
    uint32_t *reg;

    offset = fault_get_address(fault) - vgic->dist_paddr;
    action = vgic_rdist_get_action(gic_rdist, offset);
    reg = (void*)((void*)gic_rdist + (offset - offset % 4));

    /* Read fault */
    if (fault_is_read(fault)) {
        fault_set_data(fault, *reg);
        return advance_fault(fault);
    } else {
        switch (action) {
        case ACTION_READONLY:
            return ignore_fault(fault);
        default:
            DDIST("Unknown action on offset 0x%x\n", offset);
            return ignore_fault(fault);
        }
    }
    abandon_fault(fault);
    return -1;
}

memory_fault_result_t handle_vgic_rdist_sgi_ppi_fault(vm_t *vm,
                                                      vm_vcpu_t *vcpu,
                                                      uintptr_t fault_addr,
                                                      size_t fault_length,
                                                      void* cookie)
{
    enum gic_dist_action action;
    fault_t *fault = vcpu->vcpu_arch.fault;
    struct vgic_device *vgic = vm->arch.vgic;
    struct vgic_rdist_sgi_ppi_map *gic_rdist = &vgic->redistributor_sgi_ppi;
    unsigned int offset;
    uint32_t *reg, mask;

    offset = fault_get_address(fault) - vgic->dist_paddr;
    action = vgic_sgi_get_action(gic_rdist, offset);
    reg = (void*)((void*)gic_rdist + (offset - offset % 4));
    mask = fault_get_data_mask(fault);

    if (fault_is_read(fault)) {
        fault_set_data(fault, *reg);
        return advance_fault(fault);
    } else {
        uint32_t data;
        switch (action) {
        case ACTION_READONLY:
            return ignore_fault(fault);
        case ACTION_ENABLE_SET:
            data = fault_get_data(fault);

            /* Mask the data to write */
            data &= mask;
            /* Mask bits that are already set */
            data &= ~(*reg);

            while (data) {
                int irq;
                irq = CTZ(data);
                data &= ~(1U << irq);
                irq += (offset - 0x100) * 8;
                vgic_sgi_enable_irq(vgic, vcpu, irq);
            }
            return ignore_fault(fault);

        case ACTION_ENABLE_CLR:
            data = fault_get_data(fault);
            /* Mask the data to write */
            data &= mask;
            /* Mask bits that are already clear */
            data &= *reg;
            while (data) {
                int irq;
                irq = CTZ(data);
                data &= ~(1U << irq);
                irq += (offset - 0x180) * 8;
                vgic_sgi_disable_irq(vgic, irq);
            }
            return ignore_fault(fault);

        default:
            DDIST("Unknown action on offset 0x%x\n", offset);
            return ignore_fault(fault);
        }
    }
    abandon_fault(fault);
    return -1;
}
