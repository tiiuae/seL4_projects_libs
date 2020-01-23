/*
 * Copyright 2020, Data61
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
#include <sel4vm/boot.h>
#include <sel4vm/guest_irq_controller.h>

#include <sel4vmmplatsupport/guest_vcpu_util.h>
#include <sel4vmmplatsupport/arch/guest_vcpu_fault.h>
#include <sel4vmmplatsupport/arch/irq_defs.h>

static void vppi_event_ack(vm_vcpu_t *vcpu, int irq, void *cookie) {
    seL4_Error err = seL4_ARM_VCPU_AckVPPI(vcpu->vcpu.cptr, (seL4_Word)irq);
    if (err) {
        ZF_LOGE("Failed to ACK VPPI: VCPU VPPIAck invocation failed");
    }
}

static void sgi_ack(vm_vcpu_t *vcpu, int irq, void *cookie) {}

vm_vcpu_t *create_vmm_plat_vcpu(vm_t *vm, int priority) {
    int err;
    vm_vcpu_t *vm_vcpu = vm_create_vcpu(vm, priority);
    if (vm_vcpu == NULL) {
        ZF_LOGE("Failed to create platform vcpu");
        return NULL;
    }
    err = vm_register_unhandled_vcpu_fault_callback(vm_vcpu, vmm_handle_arm_vcpu_exception, NULL);
    if (err) {
        ZF_LOGE("Failed to register vcpu platform fault callback handlers");
        return NULL;
    }
    err = vm_register_irq(vm_vcpu, PPI_VTIMER_IRQ, &vppi_event_ack, NULL);
    if (err == -1) {
        ZF_LOGE("Failed to register vcpu virtual time irq");
        return NULL;
    }

    err = vm_register_irq(vm_vcpu, SGI_RESCHEDULE_IRQ, &sgi_ack, NULL);
    if (err == -1) {
        ZF_LOGE("Failed to register vcpu sgi 0 irq");
        return NULL;
    }

    err = vm_register_irq(vm_vcpu, SGI_FUNC_CALL, &sgi_ack, NULL);
    if (err == -1) {
        ZF_LOGE("Failed to register vcpu sgi 1 irq");
        return NULL;
    }

    return vm_vcpu;
}
