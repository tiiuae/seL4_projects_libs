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

#pragma once

enum vmm_lapic_state {
    LAPIC_STATE_NEW,
    LAPIC_STATE_WAITSIPI,
    LAPIC_STATE_RUN
};

#if 0
struct vmm_timer {
    struct hrtimer timer;
    int64_t period;                 /* unit: ns */
    uint32_t timer_mode_mask;
    uint64_t tscdeadline;
    atomic_t pending;           /* accumulated triggered timers */
};
#endif

typedef struct vmm_lapic {
    uint32_t apic_base; // BSP flag is ignored in this

    //struct vmm_timer lapic_timer;
    uint32_t divide_count;

    bool irr_pending;
    /* Number of bits set in ISR. */
    int16_t isr_count;
    /* The highest vector set in ISR; if -1 - invalid, must scan ISR. */
    int highest_isr_cache;
    /**
     * APIC register page.  The layout matches the register layout seen by
     * the guest 1:1, because it is accessed by the vmx microcode. XXX ???
     * Note: Only one register, the TPR, is used by the microcode.
     */
    void *regs;
    unsigned int sipi_vector;

    enum vmm_lapic_state state;
    int arb_prio;
} vmm_lapic_t;

int vmm_apic_enabled(vmm_lapic_t *apic);

int vmm_create_lapic(vm_vcpu_t *vcpu, int enabled);
void vmm_free_lapic(vm_vcpu_t *vcpu);

int vmm_apic_has_interrupt(vm_vcpu_t *vcpu);
int vmm_apic_get_interrupt(vm_vcpu_t *vcpu);

void vmm_apic_consume_extints(vm_vcpu_t *vcpu, int (*get)(void));

/* MSR functions */
void vmm_lapic_set_base_msr(vm_vcpu_t *vcpu, uint32_t value);
uint32_t vmm_lapic_get_base_msr(vm_vcpu_t *vcpu);

int vmm_apic_local_deliver(vm_vcpu_t *vcpu, int lvt_type);
int vmm_apic_accept_pic_intr(vm_vcpu_t *vcpu);

memory_fault_result_t apic_fault_callback(vm_t *vm, uintptr_t fault_addr, size_t fault_length,
        void *cookie, guest_memory_arch_data_t arch_data);

uint64_t vmm_get_lapic_tscdeadline_msr(vm_vcpu_t *vcpu);
void vmm_set_lapic_tscdeadline_msr(vm_vcpu_t *vcpu, uint64_t data);

