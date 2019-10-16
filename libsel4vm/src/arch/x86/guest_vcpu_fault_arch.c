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

#include <sel4vm/guest_vcpu_fault.h>

#include <sel4vm/vmm.h>
#include <sel4vm/platform/vmcs.h>
#include <sel4vm/processor/decode.h>

#include "guest_state.h"

seL4_Word get_vcpu_fault_address(vm_vcpu_t *vcpu) {
    return vmm_guest_exit_get_physical(vcpu->vcpu_arch.guest_state);
}

seL4_Word get_vcpu_fault_ip(vm_vcpu_t *vcpu) {
    return vmm_guest_state_get_eip(vcpu->vcpu_arch.guest_state);
}

seL4_Word get_vcpu_fault_data(vm_vcpu_t *vcpu) {
    int reg;
    uint32_t imm;
    int size;
    vmm_decode_ept_violation(vcpu, &reg, &imm, &size);
    int vcpu_reg = vmm_decoder_reg_mapw[reg];
    return vmm_read_user_context(vcpu->vcpu_arch.guest_state, vcpu_reg);
}

size_t get_vcpu_fault_size(vm_vcpu_t *vcpu) {
    int reg;
    uint32_t imm;
    int size;
    vmm_decode_ept_violation(vcpu, &reg, &imm, &size);
    return size;
}

seL4_Word get_vcpu_fault_data_mask(vm_vcpu_t *vcpu) {
    size_t size = get_vcpu_fault_size(vcpu);
    seL4_Word addr = get_vcpu_fault_address(vcpu);
    seL4_Word mask = 0;
    switch (size) {
        case sizeof(uint8_t):
            mask = 0x000000ff;
            break;
        case sizeof(uint16_t):
            mask = 0x0000ffff;
            break;
        case sizeof(uint32_t):
            mask = 0xffffffff;
            break;
        default:
            ZF_LOGE("Invalid fault size");
            return 0;
    }
    mask <<= (addr & 0x3) * 8;
    return mask;
}

bool is_vcpu_read_fault(vm_vcpu_t *vcpu) {
    unsigned int qualification = vmm_guest_exit_get_qualification(vcpu->vcpu_arch.guest_state);
    return true ? qualification & BIT(0) : false;
}

int set_vcpu_fault_data(vm_vcpu_t *vcpu, seL4_Word data) {
    int reg;
    uint32_t imm;
    int size;
    vmm_decode_ept_violation(vcpu, &reg, &imm, &size);
    int vcpu_reg = vmm_decoder_reg_mapw[reg];
    vmm_set_user_context(vcpu->vcpu_arch.guest_state, vcpu_reg, data);
    return 0;
}

void advance_vcpu_fault(vm_vcpu_t *vcpu) {
    vmm_guest_exit_next_instruction(vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
}
