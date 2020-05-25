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

#include "vgic.h"
#include "virq.h"

int vm_v3_inject_irq(vm_vcpu_t *vcpu, int irq)
{
    struct vgic_device *vgic = vcpu->vm->arch.vgic;

    if (irq >= GIC_SPI_IRQ_MIN) {
	    vgic_dist_set_pending_irq(vgic, vcpu, irq);
    } else {
        vgic_sgi_set_pending_irq(vgic, vcpu, irq);
    }

    if (!fault_handled(vcpu->vcpu_arch.fault) && fault_is_wfi(vcpu->vcpu_arch.fault)) {
        ignore_fault(vcpu->vcpu_arch.fault);
    }

    return 0;
}

int handle_vgic_v3_maintenance(vm_vcpu_t *vcpu, int idx)
{
    printf("IRQ maintenance\n");
    /*struct vgic_device *vgic = vcpu->vm->arch.vgic;
    struct virq_handle **lr;
    int i;

    assert(vgic);
    lr = vgic->irq[vcpu->vcpu_id];
    assert(lr[idx]);

    for (i = 0; i < 4; i++) {
        if (idx & BIT(i)) {
            assert(vgic->irq[i]);
            if (vgic->irq[i]->virq >= GIC_SPI_IRQ_MIN) {
                vgic_dist_clr_pending_irq(vgic, vcpu, vgic->irq[i]->virq);
            } else {
                sgi_set_pending(vgic_priv_get_rdist_sgi(d), vgic->irq[i]->virq, false);
            }
           virq_ack(vgic->irq[i]);
           vgic->irq[i] = NULL;
        }
    }
    vgic_handle_overflow(vgic, vcpu);
    */
    return 0;
}

int vm_install_vgic_v3(vm_t *vm, struct vm_irq_controller_params *params)
{
    struct vgic_device *vgic = NULL;

    if (!(vgic = malloc(sizeof(*vgic)))) {
        ZF_LOGE("Unable to malloc memory for vgic");
        return -1;
    }
    vgic_virq_init(vgic);
    vgic->version = VM_GIC_V3;

    vgic->dist_paddr = params->gic_dist_paddr;
    vgic->rdist_paddr = params->gic_rdist_paddr;
    vgic->rdist_sgi_ppi_paddr = params->gic_rdist_sgi_ppi_paddr;

    if (       !vm_reserve_memory_at(vm, params->gic_dist_paddr, params->gic_dist_size, handle_vgic_dist_fault, NULL)
            || !vm_reserve_memory_at(vm, params->gic_rdist_paddr, params->gic_rdist_size, handle_vgic_rdist_fault, NULL)
            || !vm_reserve_memory_at(vm, params->gic_rdist_sgi_ppi_paddr, params->gic_rdist_sgi_ppi_size, handle_vgic_rdist_sgi_ppi_fault, NULL)) {
        ZF_LOGE("Error installing vgic");
        free(vgic);
        return -1;
    }

    vgic_dist_v3_reset(&vgic->distributor);
    vgic_rdist_reset(&vgic->redistributor);
    vgic_rdist_sgi_ppi_reset(&vgic->redistributor_sgi_ppi);
    vm->arch.vgic = vgic;
    return 0;
}
