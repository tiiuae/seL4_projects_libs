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

/* Debugging helper functions used by VMM lib.
 *     Authors:
 *         Qian Ge
 */

#include <stdio.h>
#include <stdlib.h>

#include <sel4/sel4.h>

#include <sel4vm/guest_vm.h>
#include "sel4vm/debug.h"

/* Print out the context of a guest OS thread. */
void vmm_print_guest_context(int level, vm_vcpu_t *vcpu) {
    DPRINTF(level, "================== GUEST OS CONTEXT =================\n");

    DPRINTF(level, "exit info : reason 0x%x    qualification 0x%x   instruction len 0x%x interrupt info 0x%x interrupt error 0x%x\n",
                    vmm_guest_exit_get_reason(&vcpu->vcpu_arch.guest_state), vmm_guest_exit_get_qualification(&vcpu->vcpu_arch.guest_state), vmm_guest_exit_get_int_len(&vcpu->vcpu_arch.guest_state), vmm_vmcs_read(vcpu->vcpu.cptr, VMX_DATA_EXIT_INTERRUPT_INFO), vmm_vmcs_read(vcpu->vcpu.cptr, VMX_DATA_EXIT_INTERRUPT_ERROR));
    DPRINTF(level, "            guest physical 0x%x     rflags 0x%x \n",
                   vmm_guest_exit_get_physical(&vcpu->vcpu_arch.guest_state), vmm_guest_state_get_rflags(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr));
    DPRINTF(level, "            guest interruptibility 0x%x   control entry 0x%x\n",
                   vmm_guest_state_get_interruptibility(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr), vmm_guest_state_get_control_entry(&vcpu->vcpu_arch.guest_state));

    DPRINTF(level, "eip 0x%8x\n",
                   vmm_guest_state_get_eip(&vcpu->vcpu_arch.guest_state));
    DPRINTF(level, "eax 0x%8x         ebx 0x%8x      ecx 0x%8x\n",
                   vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EAX), vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EBX), vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_ECX));
    DPRINTF(level, "edx 0x%8x         esi 0x%8x      edi 0x%8x\n",
                   vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EDX), vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_ESI), vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EDI));
    DPRINTF(level, "ebp 0x%8x\n",
                   vmm_read_user_context(&vcpu->vcpu_arch.guest_state, USER_CONTEXT_EBP));

    DPRINTF(level, "cr0 0x%x      cr3 0x%x   cr4 0x%x\n", vmm_guest_state_get_cr0(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr), vmm_guest_state_get_cr3(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr), vmm_guest_state_get_cr4(&vcpu->vcpu_arch.guest_state, vcpu->vcpu.cptr));
}