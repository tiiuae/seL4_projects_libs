/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_memory.h>

/**
 * Check whether a region is a subregion of another region
 * @param {uintptr_t} start           Start of parent region
 * @param {size_t} size               Size of parent region
 * @param {uintptr_t} subreg_start    Start of potential subregion
 * @param {size_t} subreg_size        Size of potential subregion
 * @return                            True if subregion fits in parent completely, false otherwise
 */
static inline bool is_subregion(uintptr_t start, size_t size,
                                uintptr_t subreg_start, size_t subreg_size)
{
    return (subreg_start >= start) && (subreg_size <= size) &&
           (subreg_start - start <= size - subreg_size);
}

/**
 * Find reservation based on address
 * @param {vm_t *} vm               A handle to the VM
 * @param {uintptr_t} addr          Address to be searched for
 * @return                          Reservation containing given address, NULL if not found
 */
vm_memory_reservation_t *vm_reservation_find_by_addr(vm_t *vm, uintptr_t addr);

/**
 * Return guest memory info struct for reservation
 * @param {vm_memory_reservation_t *} reservation    A handle to reservation
 * @return                                           Guest memory info struct for reservation
 */
vm_mem_t *vm_reservation_guest_memory(vm_memory_reservation_t *reservation);

/***
 * @function vm_reservation_map(reservation)
 * Map a reservation immediately into the VM's virtual address space -- note that
 * the map iterator and its cookie must be set with vm_reservation_map_lazy() before
 * calling this.
 * @param {vm_memory_reservation_t *} reservation       Pointer to reservation object being mapped
 * @return                                              -1 on failure otherwise 0 for success
 */
int vm_reservation_map(vm_memory_reservation_t *reservation);

/***
 * @function vm_reservation_map_lazy(reservation)
 * Create a request for deferred mapping of reservation into the VM's virtual address space.
 * If deferred mapping is disabled, mapping happens immediately before returning.
 * @param {vm_memory_reservation_t *} reservation       Pointer to reservation object being mapped
 * @return                                              -1 on failure otherwise 0 for success
 */
int vm_reservation_map_lazy(vm_memory_reservation_t *reservation,
                            memory_map_iterator_fn map_iterator,
                            void *cookie);

/**
 * Handle a vm memory fault through searching previously created reservations and invoking the appropriate fault callback
 * @param {vm_t *} vm               A handle to the VM
 * @param {vm_vcpu_t *} vcpu        A handle to the faulting vcpu
 * @param {uintptr_t} addr          Faulting address
 * @param {size_t} size             Size of the faulting region
 * @return                          Fault handling status code: HANDLED, UNHANDLED, RESTART, ERROR
 */
memory_fault_result_t vm_memory_handle_fault(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t addr, size_t size);

/**
 * Map a vm memory reservation - this invokation is performed immediately (mapping is not deferred)
 * @param {vm_t *} vm                                   A handle to the VM
 * @param {vm_memory_reservation_t *} vm_reservation    A handle to the VM reservation being mapped
 * @param {memory_map_iterator_fn} map_iterator         Pointer to the map iterator function for retrieving reservation frames
 * @param {void *} map_cookie                           Cookie to pass onto map iterator
 * @return                                              0 on success, -1 on error
 */
int map_vm_memory_reservation(vm_t *vm, vm_memory_reservation_t *vm_reservation,
                              memory_map_iterator_fn map_iterator, void *map_cookie);
