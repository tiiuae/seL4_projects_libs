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
/*
 * This component controls and maintains the GIC for the VM.
 * IRQs must be registered at init time with vm_virq_new(...)
 * This function creates and registers an IRQ data structure which will be used for IRQ maintenance
 * b) ENABLING: When the VM enables the IRQ, it checks the pending flag for the VM.
 *   - If the IRQ is not pending, we either
 *        1) have not received an IRQ so it is still enabled in seL4
 *        2) have received an IRQ, but ignored it because the VM had disabled it.
 *     In either case, we simply ACK the IRQ with seL4. In case 1), the IRQ will come straight through,
       in case 2), we have ACKed an IRQ that was not yet pending anyway.
 *   - If the IRQ is already pending, we can assume that the VM has yet to ACK the IRQ and take no further
 *     action.
 *   Transitions: b->c
 * c) PIRQ: When an IRQ is received from seL4, seL4 disables the IRQ and sends an async message. When the VMM
 *    receives the message.
 *   - If the IRQ is enabled, we set the pending flag in the VM and inject the appropriate IRQ
 *     leading to state d)
 *   - If the IRQ is disabled, the VMM takes no further action, leading to state b)
 *   Transitions: (enabled)? c->d :  c->b
 * d) When the VM acknowledges the IRQ, an exception is raised and delivered to the VMM. When the VMM
 *    receives the exception, it clears the pending flag and acks the IRQ with seL4, leading back to state c)
 *    Transition: d->c
 * g) When/if the VM disables the IRQ, we may still have an IRQ resident in the GIC. We allow
 *    this IRQ to be delivered to the VM, but subsequent IRQs will not be delivered as seen by state c)
 *    Transitions g->c
 *
 *   NOTE: There is a big assumption that the VM will not manually manipulate our pending flags and
 *         destroy our state. The affects of this will be an IRQ that is never acknowledged and hence,
 *         will never occur again.
 */

#include "vgic.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <utils/arith.h>
#include <vka/vka.h>
#include <vka/capops.h>

#include <sel4vm/gen_config.h>
#include <sel4vm/guest_vm.h>
#include <sel4vm/boot.h>
#include <sel4vm/guest_memory.h>
#include <sel4vm/guest_irq_controller.h>
#include <sel4vm/guest_vm_util.h>

#include "vm.h"
#include "../fault.h"

#define GIC_V2
#include "vdist.h"

//#define DEBUG_IRQ
//#define DEBUG_DIST

#ifdef DEBUG_IRQ
#define DIRQ(...) do{ printf("VDIST: "); printf(__VA_ARGS__); }while(0)
#else
#define DIRQ(...) do{}while(0)
#endif

#define STRUCT_OFFSET(s, m) ((void*)&((s)->m) - (void*)(s))
#define STRUCT_RANGE(offset, s, m) ((offset) >= STRUCT_OFFSET(s, m) && (offset) < STRUCT_OFFSET(s, m) + sizeof((s)->m))
#define IRQ_BASE(offset, dist, enabler) (((offset) - STRUCT_OFFSET(dist, enabler)) * 8)
#define OFFSET_TO_IDX(offset, dist, reg) (((offset) - STRUCT_OFFSET(dist, reg)) / 4)

int handle_vgic_maintenance(vm_vcpu_t *vcpu, int idx)
{
    /* STATE d) */
    struct vgic_device *vgic = vcpu->vm->arch.vgic;
    struct vgic_v2_dist_map *gic_dist;
    struct virq_handle **lr;

    assert(vgic);
    gic_dist = &vgic->reg_v2.distributor;
    assert(gic_dist);
    lr = vgic->irq[vcpu->vcpu_id];
    assert(lr[idx]);

    /* Clear pending */
    DIRQ("Maintenance IRQ %d\n", lr[idx]->virq);
    set_pending(gic_dist, lr[idx]->virq, false, vcpu->vcpu_id);
    virq_ack(vcpu, lr[idx]);

    /* Check the overflow list for pending IRQs */
    lr[idx] = NULL;
    vgic_handle_overflow(vgic, vcpu);
    return 0;
}

static enum gic_dist_action vgic_dist_get_action(struct vgic_v2_dist_map *dist_map, unsigned int offset,
                                                 fault_t *fault) {
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
        return ACTION_PASSTHROUGH;
    } else {
        return ACTION_READONLY;
    }
}

static memory_fault_result_t handle_vgic_dist_sgi_fault(struct vgic_device *vgic, vm_vcpu_t *vcpu, fault_t *fault,
                                                        enum gic_dist_action action, unsigned int offset) {
    uint32_t reg;
    struct vgic_v2_dist_map *dist = &vgic->reg_v2.distributor;
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

static memory_fault_result_t handle_vgic_dist_fault(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr,
                                                    size_t fault_length,
                                                    void *cookie)
{
    enum gic_dist_action action;
    fault_t *fault = vcpu->vcpu_arch.fault;
    struct vgic_device *vgic = vm->arch.vgic;
    struct vgic_v2_dist_map *dist = &vgic->reg_v2.distributor;
    unsigned int offset;
    uint32_t *reg;

    offset = fault_get_address(fault) - vgic->reg_v2.dist_paddr;
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
                data = fault_get_data(fault);
                data &= fault_get_data_mask(fault);
                data &= ~(*reg);
                while (data) {
                    int irq = IRQ_BASE(offset, dist, enable_set) + CTZ(data);
                    data &= ~(1U << CTZ(data));
                    vgic_dist_enable_irq(vgic, vcpu, irq);
                }
                return ignore_fault(fault);
            case ACTION_ENABLE_CLR:
                data = fault_get_data(fault);
                data &= fault_get_data_mask(fault);
                data &= *reg;
                while (data) {
                    int irq = IRQ_BASE(offset, dist, enable_clr) + CTZ(data);
                    data &= ~(1U << CTZ(data));
                    vgic_dist_disable_irq(vgic, vcpu, irq);
                }
                return ignore_fault(fault);
            case ACTION_PENDING_SET:
                data = fault_get_data(fault);
                data &= fault_get_data_mask(fault);
                data &= ~(*reg);
                while (data) {
                    int irq = IRQ_BASE(offset, dist, pending_set) + CTZ(data);
                    data &= ~(1U << CTZ(data));
                    vgic_dist_set_pending_irq(vgic, vcpu, irq);
                }
                return ignore_fault(fault);
            case ACTION_PENDING_CLR:
                data = fault_get_data(fault);
                data &= fault_get_data_mask(fault);
                data &= *reg;
                while (data) {
                    int irq = IRQ_BASE(offset, dist, pending_clr) + CTZ(data);
                    data &= ~(1U << CTZ(data));
                    vgic_dist_clr_pending_irq(vgic, vcpu, irq);
                }
                return ignore_fault(fault);
            default:
                ZF_LOGF("Discarding unknown action on vgic distributor: 0x%x", offset);
                return ignore_fault(fault);
        }
    }
}

static void vgic_dist_reset(struct vgic_v2_dist_map *gic_dist)
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

int vm_register_irq(vm_vcpu_t *vcpu, int irq, irq_ack_fn_t ack_fn, void *cookie)
{
    struct virq_handle *virq_data;
    struct vgic_device *vgic;
    int err;

    vgic = vcpu->vm->arch.vgic;
    assert(vgic);

    virq_data = calloc(1, sizeof(*virq_data));
    if (!virq_data) {
        return -1;
    }
    virq_init(virq_data, irq, ack_fn, cookie);
    err = virq_add(vcpu, vgic, virq_data);
    if (err) {
        free(virq_data);
        return -1;
    }
    return 0;
}

int vm_inject_irq(vm_vcpu_t *vcpu, int irq)
{
    struct vgic_device *vgic = vcpu->vm->arch.vgic;
    // vm->lock();

    DIRQ("VM received IRQ %d\n", irq);

    assert(vgic);
    int err = vgic_dist_set_pending_irq(vgic, vcpu, irq);

    if (!fault_handled(vcpu->vcpu_arch.fault) && fault_is_wfi(vcpu->vcpu_arch.fault)) {
        ignore_fault(vcpu->vcpu_arch.fault);
    }

    // vm->unlock();

    return err;
}

static memory_fault_result_t handle_vgic_vcpu_fault(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr,
                                                    size_t fault_length,
                                                    void *cookie)
{
    /* We shouldn't fault on the vgic vcpu region as it should be mapped in
     * with all rights */
    return FAULT_ERROR;
}

struct vgic_vcpu_iterator_cookie {
    vm_t *vm;
    struct vm_irq_controller_params *params;
};

static vm_frame_t vgic_vcpu_iterator(uintptr_t addr, void *data)
{
    int err;
    cspacepath_t frame;
    vm_frame_t frame_result = { seL4_CapNull, seL4_NoRights, 0, 0 };
    struct vgic_vcpu_iterator_cookie *cookie = data;
    vm_t *vm = cookie->vm;
    struct vm_irq_controller_params *params = cookie->params;

    err = vka_cspace_alloc_path(vm->vka, &frame);
    if (err) {
        printf("Failed to allocate cslot for vgic vcpu\n");
        return frame_result;
    }
    seL4_Word vka_cookie;
    err = vka_utspace_alloc_at(vm->vka, &frame, kobject_get_type(KOBJECT_FRAME, 12), 12, params->gic_vcpu_paddr, &vka_cookie);
    if (err) {
        err = simple_get_frame_cap(vm->simple, (void *)params->gic_vcpu_paddr, 12, &frame);
        if (err) {
            ZF_LOGE("Failed to find device cap for vgic vcpu\n");
            return frame_result;
        }
    }
    frame_result.cptr = frame.capPtr;
    frame_result.rights = seL4_AllRights;
    frame_result.vaddr = params->gic_cpu_paddr;
    frame_result.size_bits = seL4_PageBits;
    return frame_result;
}

/*
 * 1) completely virtual the distributor
 * 2) remap vcpu to cpu. Full access
 */
int vm_install_vgic_v2(vm_t *vm, struct vm_irq_controller_params *params)
{
    struct vgic_device *vgic = NULL;
    struct vgic_vcpu_iterator_cookie cookie = {vm, params};
    vm_memory_reservation_t *vgic_vcpu_reservation;

    if (!(vgic = malloc(sizeof(*vgic)))) {
        assert(!"Unable to malloc memory for VGIC");
        return -1;
    }
    vgic_virq_init(vgic);
    vgic->version = VM_GIC_V2;

    vgic->reg_v2.dist_paddr = params->gic_dist_paddr;
    /* vgic->reg_v2.dist_size  = params->gic_dist_size; */

    if (       !vm_reserve_memory_at(vm, params->gic_dist_paddr, PAGE_SIZE_4K, handle_vgic_dist_fault, NULL)
            || !(vgic_vcpu_reservation = vm_reserve_memory_at(vm, params->gic_cpu_paddr, 0x1000, handle_vgic_vcpu_fault, NULL))
            || vm_map_reservation(vm, vgic_vcpu_reservation, vgic_vcpu_iterator, &cookie)) {
        ZF_LOGE("Error installing vgic");
        free(vgic);
        return -1;
    }

    vgic_dist_reset(&vgic->reg_v2.distributor);
    vm->arch.vgic = vgic;
    return 0;
}

int vm_vgic_maintenance_handler(vm_vcpu_t *vcpu)
{
    int idx;
    int err;
    idx = seL4_GetMR(seL4_UnknownSyscall_ARG0);
    /* Currently not handling spurious IRQs */
    assert(idx >= 0);

    err = handle_vgic_maintenance(vcpu, idx);
    if (!err) {
        seL4_MessageInfo_t reply;
        reply = seL4_MessageInfo_new(0, 0, 0, 0);
        seL4_Reply(reply);
    }
    return VM_EXIT_HANDLED;
}
