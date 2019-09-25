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

#pragma once

#include <stdbool.h>

#include <sel4/sel4.h>
#include <sel4vm/guest_vm.h>

/*
 * Get current fault address of vcpu
 * @param vcpu  Handle to vcpu
 * @return      Current fault address of vcpu
 */
seL4_Word get_vcpu_fault_address(vm_vcpu_t *vcpu);

/*
 * Get instruction pointer of current vcpu fault
 * @param vcpu  Handle to vcpu
 * @return      Intruction pointer of vcpu fault
 */
seL4_Word get_vcpu_fault_ip(vm_vcpu_t *vcpu);

/*
 * Get the data of the current vcpu fault
 * @param vcpu  Handle to vcpu
 * @return      Data of vcpu fault
 */
seL4_Word get_vcpu_fault_data(vm_vcpu_t *vcpu);

/*
 * Get data mask of the current vcpu fault
 * @param vcpu  Handle to vcpu
 * @return      Data mask of vcpu fault
 */
seL4_Word get_vcpu_fault_data_mask(vm_vcpu_t *vcpu);

/*
 * Get access size of the current vcpu fault
 * @param vcpu  Handle to vcpu
 * @return      Access size of vcpu fault
 */
size_t get_vcpu_fault_size(vm_vcpu_t *vcpu);

/*
 * Is current vcpu fault a read fault
 * @param vcpu  Handle to vcpu
 * @return      True if read fault, False if write fault
 */
bool is_vcpu_read_fault(vm_vcpu_t *vcpu);

/*
 * Set the data of the current vcpu fault
 * @param vcpu  Handle to vcpu
 * @param data  Data to set for current vcpu fault
 * @return 0 for success, otherwise -1 for error
 */
int set_vcpu_fault_data(vm_vcpu_t *vcpu, seL4_Word data);

/*
 * Emulate a read or write fault on a given data value
 * @param vcpu  Handle to vcpu
 * @param data  Data to perform emulate fault on
 * @return      Emulation result of vcpu fault over given data value
 */
seL4_Word emulate_vcpu_fault(vm_vcpu_t *vcpu, seL4_Word data);

/*
 * Advance the current vcpu fault to the next stage/instruction
 * @param vcpu  Handle to vcpu
 */
void advance_vcpu_fault(vm_vcpu_t *vcpu);
