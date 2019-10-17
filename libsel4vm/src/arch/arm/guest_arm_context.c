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

#include <sel4/sel4.h>
#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_vm_util.h>
#include <sel4vm/guest_arm_context.h>

int vm_set_thread_context(vm_vcpu_t *vcpu, seL4_UserContext context) {
    seL4_CPtr tcb = vm_get_tcb(vcpu->vm);
    int err = seL4_TCB_WriteRegisters(tcb, false, 0, sizeof(context) / sizeof(context.pc), &context);
    if (err) {
        ZF_LOGE("Failed to set thread context: Unable to write TCB registers");
        return -1;
    }
    return 0;
}

int vm_set_thread_context_reg(vm_vcpu_t *vcpu, unsigned int reg, uintptr_t value) {
    seL4_UserContext regs;
    seL4_CPtr tcb = vm_get_tcb(vcpu->vm);
    int err = seL4_TCB_ReadRegisters(tcb, false, 0, sizeof(regs) / sizeof(regs.pc), &regs);
    if (err) {
        ZF_LOGE("Failed to set thread context reg: Unable to read TCB registers");
        return -1;
    }
    (&regs.pc)[reg] = value;
    err = seL4_TCB_WriteRegisters(tcb, false, 0, sizeof(regs) / sizeof(regs.pc), &regs);
    if (err) {
        ZF_LOGE("Failed to set thread context register: Unable to write new register to TCB");
        return -1;
    }
    return 0;
}

int vm_get_thread_context(vm_vcpu_t *vcpu, seL4_UserContext *context) {
    seL4_UserContext regs;
    seL4_CPtr tcb = vm_get_tcb(vcpu->vm);
    int err = seL4_TCB_ReadRegisters(tcb, false, 0, sizeof(regs) / sizeof(regs.pc), &regs);
    if (err) {
        ZF_LOGE("Failed to get thread context: Unable to read TCB registers");
        return -1;
    }
    *context = regs;
    return 0;
}

int vm_get_thread_context_reg(vm_vcpu_t *vcpu, unsigned int reg, uintptr_t *value) {
    seL4_UserContext regs;
    seL4_CPtr tcb = vm_get_tcb(vcpu->vm);
    int err = seL4_TCB_ReadRegisters(tcb, false, 0, sizeof(regs) / sizeof(regs.pc), &regs);
    if (err) {
        ZF_LOGE("Failed to get thread context register: Unable to read TCB registers");
        return -1;
    }
    *value =  (&regs.pc)[reg];
    return 0;
}

int vm_set_arm_vcpu_reg(vm_vcpu_t *vcpu, seL4_Word reg, uintptr_t value) {
    int err = seL4_ARM_VCPU_WriteRegs(vcpu->vcpu.cptr, reg, value);
    if (err) {
        ZF_LOGE("Failed to set VCPU register: Write registers failed");
        return -1;
    }
    return 0;
}

int vm_get_arm_vcpu_reg(vm_vcpu_t *vcpu, seL4_Word reg, uintptr_t *value) {
    seL4_ARM_VCPU_ReadRegs_t res = seL4_ARM_VCPU_ReadRegs(vcpu->vcpu.cptr, reg);
    if (res.error) {
        ZF_LOGF("Read registers failed");
        return -1;
    }
    *value = res.value;
    return 0;
}
