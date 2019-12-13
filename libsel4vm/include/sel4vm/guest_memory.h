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

#include <stdint.h>
#include <sel4/sel4.h>

#include <sel4vm/arch/guest_memory_arch.h>

typedef struct vm vm_t;
typedef struct vm_vcpu vm_vcpu_t;

typedef struct vm_frame {
    seL4_CPtr cptr;
    seL4_CapRights_t rights;
    uintptr_t vaddr;
    size_t size_bits;
} vm_frame_t;

typedef enum memory_fault_result {
    FAULT_HANDLED,
    FAULT_UNHANDLED,
    FAULT_RESTART,
    FAULT_IGNORE,
    FAULT_ERROR
} memory_fault_result_t;

typedef memory_fault_result_t (*memory_fault_callback_fn)(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr, size_t fault_length,
        void *cookie);
typedef vm_frame_t (*memory_map_iterator_fn)(uintptr_t addr, void *cookie);

typedef struct vm_memory_reservation vm_memory_reservation_t;
typedef struct vm_memory_reservation_cookie vm_memory_reservation_cookie_t;

/**
 * Handle a vm memory fault through searching previously created reservations and invoking the appropriate fault callback
 * @param[in] vm                    A handle to the VM
 * @param[in] vcpu                  A handle to the faulting vcpu
 * @param[in] addr                  Faulting address
 * @param[in] size                  Size of the faulting region
 * @return                          Fault handling status code: HANDLED, UNHANDLED, RESTART, ERROR
 */
memory_fault_result_t vm_memory_handle_fault(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t addr, size_t size);

/**
 * Reserve a region of the VM's memory at a given base address
 * @param[in] vm                    A handle to the VM
 * @param[in] addr                  Base address of the memory region being reserved
 * @param[in] size                  Size of the memory region being reserved
 * @param[in] fault_callback        Callback function that will be invoked if memory region is faulted on
 * @param[in] cookie                User cookie to pass onto to callback
 * @return                          NULL on failure otherwise a pointer to a reservation object representing the reserved region
 */
vm_memory_reservation_t *vm_reserve_memory_at(vm_t *vm, uintptr_t addr, size_t size,
        memory_fault_callback_fn fault_callback, void *cookie);

/**
 * Reserve an anonymous region of the VM's memory. This uses memory previously made anonymous
 * through the `vm_memory_make_anon` function.
 * @param[in] vm                    A handle to the VM
 * @param[in] size                  Size of the anoymous emory region being reserved
 * @param[in] fault_callback        Callback function that will be invoked if memory region is faulted on
 * @param[in] cookie                User cookie to pass onto to callback
 * @param[in] addr                  Pointer that will be set with the base address of the reserved anonymous region
 * @return                          NULL on failure otherwise a pointer to a reservation object representing the reserved region
 */
vm_memory_reservation_t *vm_reserve_anon_memory(vm_t *vm, size_t size,
        memory_fault_callback_fn fault_callback, void *cookie, uintptr_t *addr);

/**
 * Create an anoymous region of the VM's memory. This claims a region of VM memory that can be used for the creation
 * of anonymous reservations (achieved by calling 'vm_reserve_anon_memory').
 * @param[in] vm                    A handle to the VM
 * @param[in] addr                  Base address of the memory region being made into an anoymous reservation
 * @param[in] size                  Size of the memory region being reserved
 * @return                          -1 on failure otherwise 0 for success
 */
int vm_memory_make_anon(vm_t *vm, uintptr_t addr, size_t size);

/**
 * Free memory reservation from the VM
 * @param[in] vm                    A handle to the VM
 * @param[in] reservation           Pointer to the reservation being free'd
 * @return                          -1 on failure otherwise 0 for success
 */
int vm_free_reserved_memory(vm_t *vm, vm_memory_reservation_t *reservation);

/**
 * Map a reservation into the VM's virtual address space
 * @param[in] vm                    A handle to the VM
 * @param[in] reservation           Pointer to reservation object being mapped
 * @param[in] map_iterator          Iterator function that returns a cap to the memory region being mapped
 * @param[in] cookie                Cookie to pass onto map_iterator function
 */
int vm_map_reservation(vm_t *vm, vm_memory_reservation_t *reservation, memory_map_iterator_fn map_iterator, void *cookie);

/**
 * Get the memory region information (address & size) from a given reservation
 * @param[in] reservation           Pointer to reservation object
 * @param[in] addr                  Pointer that will be set with the address of reservation
 * @param[in] size                  Pointer that will be set with the size of reservation
 */
void vm_get_reservation_memory_region(vm_memory_reservation_t *reservation, uintptr_t *addr, size_t *size);

/**
 * Initialise a VM's memory management interface
 * @param[in] vm                    A handle to the VM
 * @return                          -1 on failure otherwise 0 for success
*/
int vm_memory_init(vm_t *vm);
