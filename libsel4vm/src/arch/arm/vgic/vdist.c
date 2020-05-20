#include "vgic.h"
#include "../fault.h"

#define GIC_ENABLE  1

#define STRUCT_OFFSET(s, m)                 ((void*)&((s)->m) - (void*)(s))
#define STRUCT_RANGE(offset, s, m)          ((offset) >= STRUCT_OFFSET(s, m) && (offset) < STRUCT_OFFSET(s, m) + sizeof((s)->m))
#define IRQ_BASE(offset, dist, enabler)     (((offset) - STRUCT_OFFSET(dist, enabler)) * 8)
#define OFFSET_TO_IDX(offset, dist, reg)    (((offset) - STRUCT_OFFSET(dist, reg)) / 4)

static enum gic_dist_action vgic_dist_get_action(struct vgic_dist_map *dist_map, unsigned int offset,
                                                 fault_t *fault)
{
    /* First handle the core-specific stuff */
    if (fault_is_read(fault)) {
        if (STRUCT_RANGE(offset, dist_map, irq_group[0])) {
            return ACTION_SGI_IRQ_GROUP;
        } else if (STRUCT_RANGE(offset, dist_map, enable_set[0])) {
            return ACTION_SGI_ENABLE_SET;
        } else if (STRUCT_RANGE(offset, dist_map, enable_clr[0])) {
            return ACTION_SGI_ENABLE_CLR;
        } else if (STRUCT_RANGE(offset, dist_map, pending_set[0])) {
            return ACTION_SGI_PENDING_SET;
        } else if (STRUCT_RANGE(offset, dist_map, pending_clr[0])) {
            return ACTION_PENDING_CLR;
        } else if (STRUCT_RANGE(offset, dist_map, active[0])) {
            return ACTION_SGI_ACTIVE;
        } else if (STRUCT_RANGE(offset, dist_map, active_clr[0])) {
            return ACTION_SGI_ACTIVE_CLR;
        } else if (STRUCT_RANGE(offset, dist_map, priority[0])) {
            return ACTION_SGI_PRIORITY;
        } else if (STRUCT_RANGE(offset, dist_map, targets[0])) {
            return ACTION_SGI_TARGETS;
        } else if (STRUCT_RANGE(offset, dist_map, _sgi_pending_clr[0])) {
            return ACTION_SGI_PENDING_CLR;
        } else if (STRUCT_RANGE(offset, dist_map, _sgi_pending_set[0])) {
            return ACTION_SGI_PENDING_SET;
        }
    }

    /* Then the generic stuff */
    if (STRUCT_RANGE(offset, dist_map, enable)) {
        return ACTION_ENABLE;
    } else if (STRUCT_RANGE(offset, dist_map, irq_group)) {
        return ACTION_PASSTHROUGH;
    } else if (STRUCT_RANGE(offset, dist_map, enable_set)) {
        return ACTION_ENABLE_SET;
    } else if (STRUCT_RANGE(offset, dist_map, enable_clr)) {
        return ACTION_ENABLE_CLR;
    } else if (STRUCT_RANGE(offset, dist_map, pending_set)) {
        return ACTION_PENDING_SET;
    } else if (STRUCT_RANGE(offset, dist_map, pending_clr)) {
        return ACTION_PENDING_CLR;
    } else if (STRUCT_RANGE(offset, dist_map, sgi_control)) {
        return ACTION_SGI_CTRL;
    } else {
        return ACTION_READONLY;
    }
}

static memory_fault_result_t handle_vgic_dist_sgi_fault(struct vgic_device *vgic, vm_vcpu_t *vcpu, fault_t *fault,
                                                        enum gic_dist_action action, unsigned int offset)
{
    uint32_t reg;
    struct vgic_dist_map *dist = &vgic->distributor;
    uint32_t mask = fault_get_data_mask(fault);

    switch (action) {
        case ACTION_SGI_IRQ_GROUP:
            reg = dist->irq_group0[vcpu->vcpu_id];
            break;
        case ACTION_SGI_ENABLE_SET:
            reg = dist->enable_set0[vcpu->vcpu_id];
            break;
        case ACTION_SGI_ENABLE_CLR:
            reg = dist->enable_clr0[vcpu->vcpu_id];
            break;
        case ACTION_SGI_PENDING_SET:
            reg = dist->pending_set0[vcpu->vcpu_id];
            break;
        case ACTION_SGI_PENDING_CLR:
            reg = dist->pending_clr0[vcpu->vcpu_id];
            break;
        case ACTION_SGI_ACTIVE:
            reg = dist->active0[vcpu->vcpu_id];
            break;
        case ACTION_SGI_ACTIVE_CLR:
            reg = dist->active_clr0[vcpu->vcpu_id];
            break;
        case ACTION_SGI_PRIORITY:
            reg = dist->priority0[vcpu->vcpu_id][OFFSET_TO_IDX(offset, dist, priority)];
            break;
        case ACTION_SGI_TARGETS:
            reg = dist->targets0[vcpu->vcpu_id][OFFSET_TO_IDX(offset, dist, targets)];
            break;
        default:
            break;
    }
    fault_set_data(fault, reg & mask);
    return advance_fault(fault);
}

memory_fault_result_t handle_vgic_dist_fault(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr,
                                             size_t fault_length,
                                             void *cookie)
{
    enum gic_dist_action action;
    fault_t *fault = vcpu->vcpu_arch.fault;
    struct vgic_device *vgic = vm->arch.vgic;
    struct vgic_dist_map *dist = &vgic->distributor;
    unsigned int offset;
    uint32_t *reg;

    offset = fault_get_address(fault) - vgic->dist_paddr;
    action = vgic_dist_get_action(dist, offset, fault);
    reg = (void*)((void*)dist + (offset - offset % 4));

    if (fault_is_read(fault)) {
        /* Need to separate SGI read actions from others since they are muliplexed between cores */
        if (action < ACTION_NUM_SGI_ACTIONS) {
            return handle_vgic_dist_sgi_fault(vgic, vcpu, fault, action, offset);
        } else {
            fault_set_data(fault, *reg);
            return advance_fault(fault);
        }
    } else {
        /* For write actions, SGI are handled within the vgic_dist_enable* functions */
        uint32_t data;
        data = fault_get_data(fault);
        data &= fault_get_data_mask(fault);
        switch (action) {
            case ACTION_READONLY:
                return ignore_fault(fault);
            case ACTION_PASSTHROUGH:
                *reg = fault_emulate(fault, *reg);
                return advance_fault(fault);
            case ACTION_ENABLE:
                data = fault_get_data(fault);
                if (data == GIC_ENABLE) {
                    dist->enable = 1;
                } else if (data == 0) {
                    dist->enable = 0;
                } else {
                    ZF_LOGF("Unknown enable register encoding");
                }
                return advance_fault(fault);
            case ACTION_ENABLE_SET:
                data &= ~(*reg);
                while (data) {
                    int irq = IRQ_BASE(offset, dist, enable_set) + CTZ(data);
                    data &= ~(1U << CTZ(data));
                    vgic_dist_enable_irq(vgic, vcpu, irq);
                }
                return ignore_fault(fault);
            case ACTION_ENABLE_CLR:
                data &= *reg;
                while (data) {
                    int irq = IRQ_BASE(offset, dist, enable_clr) + CTZ(data);
                    data &= ~(1U << CTZ(data));
                    vgic_dist_disable_irq(vgic, vcpu, irq);
                }
                return ignore_fault(fault);
            case ACTION_PENDING_SET:
                data &= ~(*reg);
                while (data) {
                    int irq = IRQ_BASE(offset, dist, pending_set) + CTZ(data);
                    data &= ~(1U << CTZ(data));
                    vgic_dist_set_pending_irq(vgic, vcpu, irq);
                }
                return ignore_fault(fault);
            case ACTION_PENDING_CLR:
                data &= *reg;
                while (data) {
                    int irq = IRQ_BASE(offset, dist, pending_clr) + CTZ(data);
                    data &= ~(1U << CTZ(data));
                    vgic_dist_clr_pending_irq(vgic, vcpu, irq);
                }
                return ignore_fault(fault);
            case ACTION_SGI_CTRL:
                ZF_LOGF("Discarding SGI control: 0x%x", data);
                return ignore_fault(fault);
            default:
                ZF_LOGF("Discarding unknown action on vgic distributor: 0x%x", offset);
                return ignore_fault(fault);
        }
    }
}

void vgic_dist_reset(struct vgic_dist_map *gic_dist)
{
    memset(gic_dist, 0, sizeof(*gic_dist));
    gic_dist->ic_type         = 0x0000fce7; /* RO */
    gic_dist->dist_ident      = 0x0200043b; /* RO */

    for (int i = 0; i < CONFIG_MAX_NUM_NODES; i++) {
        gic_dist->enable_set0[i]   = 0x0000ffff; /* 16bit RO */
        gic_dist->enable_clr0[i]   = 0x0000ffff; /* 16bit RO */
    }

    /* Reset value depends on GIC configuration */
    gic_dist->config[0]       = 0xaaaaaaaa; /* RO */
    gic_dist->config[1]       = 0x55540000;
    gic_dist->config[2]       = 0x55555555;
    gic_dist->config[3]       = 0x55555555;
    gic_dist->config[4]       = 0x55555555;
    gic_dist->config[5]       = 0x55555555;
    gic_dist->config[6]       = 0x55555555;
    gic_dist->config[7]       = 0x55555555;
    gic_dist->config[8]       = 0x55555555;
    gic_dist->config[9]       = 0x55555555;
    gic_dist->config[10]      = 0x55555555;
    gic_dist->config[11]      = 0x55555555;
    gic_dist->config[12]      = 0x55555555;
    gic_dist->config[13]      = 0x55555555;
    gic_dist->config[14]      = 0x55555555;
    gic_dist->config[15]      = 0x55555555;

    /* Configure per-processor SGI/PPI target registers */
    for (int i = 0; i < CONFIG_MAX_NUM_NODES; i++) {
        for (int j = 0; j < ARRAY_SIZE(gic_dist->targets0[i]); j++) {
            for (int irq = 0; irq < sizeof(uint32_t); irq++) {
                gic_dist->targets0[i][j] |= ((1 << i) << (irq * 8));
            }
        }
    }
    /* Deliver the SPI interrupts to the first CPU interface */
    for (int i = 0; i < ARRAY_SIZE(gic_dist->targets); i++) {
        gic_dist->targets[i] = 0x1010101;
    }

    /* identification */
    gic_dist->periph_id[4]    = 0x00000004; /* RO */
    gic_dist->periph_id[8]    = 0x00000090; /* RO */
    gic_dist->periph_id[9]    = 0x000000b4; /* RO */
    gic_dist->periph_id[10]   = 0x0000002b; /* RO */
    gic_dist->component_id[0] = 0x0000000d; /* RO */
    gic_dist->component_id[1] = 0x000000f0; /* RO */
    gic_dist->component_id[2] = 0x00000005; /* RO */
    gic_dist->component_id[3] = 0x000000b1; /* RO */
}
