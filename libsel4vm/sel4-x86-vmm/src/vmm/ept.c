/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

/*vm exits related with ept violations*/

#include <stdio.h>
#include <stdlib.h>

#include <sel4/sel4.h>

#include <sel4vm/guest_memory.h>

#include "sel4vm/debug.h"
#include "sel4vm/vmm.h"
#include "sel4vm/platform/vmcs.h"
#include "sel4vm/processor/decode.h"

#include "vm.h"

#define EPT_VIOL_READ(qual) ((qual) & BIT(0))
#define EPT_VIOL_WRITE(qual) ((qual) & BIT(1))
#define EPT_VIOL_FETCH(qual) ((qual) & BIT(2))

void print_ept_violation(vm_vcpu_t *vcpu) {
    /* Read linear address that guest is trying to access. */
    unsigned int linear_address = vmm_vmcs_read(vcpu->vcpu.cptr, VMX_DATA_GUEST_LINEAR_ADDRESS);
    printf(COLOUR_R "!!!!!!!! ALERT :: GUEST OS PAGE FAULT !!!!!!!!\n");
    printf("    Guest OS VMExit due to EPT Violation:\n");
    printf("        Linear address 0x%x.\n", linear_address);
    printf("        Guest-Physical address 0x%x.\n", vmm_guest_exit_get_physical(&vcpu->vcpu_arch.guest_state));
    printf("        Instruction pointer 0x%x.\n", vmm_guest_state_get_eip(&vcpu->vcpu_arch.guest_state));
    printf("    This is most likely due to a bug or misconfiguration.\n" COLOUR_RESET);
}

static void decode_ept_violation(vm_vcpu_t *vcpu, int *reg, uint32_t *imm, int *size) {
    /* Decode instruction */
    uint8_t ibuf[15];
    int instr_len = vmm_guest_exit_get_int_len(&vcpu->vcpu_arch.guest_state);
    vmm_fetch_instruction(vcpu,
            vmm_guest_state_get_eip(&vcpu->vcpu_arch.guest_state),
            vmm_guest_state_get_cr3(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr),
            instr_len, ibuf);

    vmm_decode_instruction(ibuf, instr_len, reg, imm, size);
}

static int unhandled_memory_fault(vm_t *vm, vm_vcpu_t *vcpu, uint32_t guest_phys,
        size_t size, bool is_read, uint32_t data, int vcpu_reg) {
    seL4_Word fault_data = 0;
    if (!is_read) {
        fault_data = data;
    }
    memory_fault_result_t fault_result = vm->mem.unhandled_mem_fault_handler(vm, guest_phys, size,
            is_read, &fault_data, UINT_MAX, vm->mem.unhandled_mem_fault_cookie);
    switch(fault_result) {
        case FAULT_ERROR:
            print_ept_violation(vcpu);
            return -1;
        case FAULT_HANDLED:
            if (is_read) {
                vmm_set_user_context(&vcpu->vcpu_arch.guest_state,
                        vcpu_reg, fault_data);
            }
            return 0;
        case FAULT_IGNORE:
            vmm_guest_exit_next_instruction(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
            return 0;
    }
    return -1;
}

/* Handling EPT violation VMExit Events. */
int vmm_ept_violation_handler(vm_vcpu_t *vcpu) {
    int err;
    uintptr_t guest_phys = vmm_guest_exit_get_physical(&vcpu->vcpu_arch.guest_state);
    unsigned int qualification = vmm_guest_exit_get_qualification(&vcpu->vcpu_arch.guest_state);

    int read = EPT_VIOL_READ(qualification);
    int write = EPT_VIOL_WRITE(qualification);
    int fetch = EPT_VIOL_FETCH(qualification);
    if (read && write) {
        /* Indicates a fault while walking EPT */
        return VM_EXIT_HANDLE_ERROR;
    }
    if (fetch) {
        /* This is not MMIO */
        return VM_EXIT_HANDLE_ERROR;
    }

    int reg;
    uint32_t imm;
    int size;
    decode_ept_violation(vcpu, &reg, &imm, &size);
    if (size != 4) {
        ZF_LOGE("Currently don't support non-32 bit accesses");
        return VM_EXIT_HANDLE_ERROR;
    }
    if (reg < 0) {
        ZF_LOGE("Invalid reg while decoding ept violation");
        return VM_EXIT_HANDLE_ERROR;
    }

    guest_memory_arch_data_t arch_data;
    uint32_t value;
    int vcpu_reg;
    if (read) {
        arch_data.is_read = true;
    } else {
        arch_data.is_read = false;
        value = imm;
        vcpu_reg = vmm_decoder_reg_mapw[reg];
        value = vmm_read_user_context(&vcpu->vcpu_arch.guest_state, vcpu_reg);
    }
    arch_data.data = &value;
    arch_data.vcpu = vcpu;

    memory_fault_result_t fault_result = vm_memory_handle_fault(vcpu->vm, guest_phys, size, arch_data);
    switch(fault_result) {
        case FAULT_ERROR:
            print_ept_violation(vcpu);
            return -1;
        case FAULT_HANDLED:
            if (read) {
                vcpu_reg = vmm_decoder_reg_mapw[reg];
                vmm_set_user_context(&vcpu->vcpu_arch.guest_state,
                        vcpu_reg, value);
            }
        case FAULT_IGNORE:
            vmm_guest_exit_next_instruction(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr);
            return VM_EXIT_HANDLED;
        case FAULT_UNHANDLED:
            if (vcpu->vm->mem.unhandled_mem_fault_handler) {
                err = unhandled_memory_fault(vcpu->vm, vcpu, guest_phys, size, read, value, vcpu_reg);
                if (err) {
                    return -1;
                }
                return 0;
            }
    }
    ZF_LOGE("Failed to handle ept fault");
    print_ept_violation(vcpu);
    return -1;
}
