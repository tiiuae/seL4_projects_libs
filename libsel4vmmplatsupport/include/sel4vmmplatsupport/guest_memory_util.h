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

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_memory.h>

/**
 * Create and map a reservation for a vka allocated frame. The allocated frame is mapped in both the vm and vmm vspace
 * @param[in] vm                    A handle to the VM
 * @param[in] addr                  Address of emulated frame
 * @param[in] rights                Rights for mapping the allocated frame into the vm's vspace
 * @param[in] alloc_fault_callback   Fault callback for allocated frame
 * @param[in] alloc_fault_cookie     Cookie for fault callback
 * @return                          Address of allocated frame in vmm vspace
 */
void *create_allocated_reservation_frame(vm_t *vm, uintptr_t addr, seL4_CapRights_t rights,
                                         memory_fault_callback_fn alloc_fault_callback, void *alloc_fault_cookie);
/**
 * Create and map a reservation for a device frame. The device frame is mapped in both the vm and vmm vspace
 * @param[in] vm                    A handle to the VM
 * @param[in] addr                  Address of emulated frame
 * @param[in] rights                Rights for mapping the device frame into the vm's vspace
 * @param[in] fault_callback        Fault callback for the frame
 * @param[in] fault_cookie          Cookie for fault callback
 * @return                          Address of device frame in vmm vspace
 */
void *create_device_reservation_frame(vm_t *vm, uintptr_t addr, seL4_CapRights_t rights,
                                      memory_fault_callback_fn fault_callback, void *fault_cookie);

/**
 * Map a guest reservation backed with untyped frames allocated from a base paddr
 * @param[in] vm                    A handle to the VM
 * @param[in] paddr                 Base paddr to allocate from
 * @param[in] reservation           Pointer to reservation object being mapped
 * @return                          -1 on failure otherwise 0 for success
 */
int map_ut_alloc_reservation_with_base_paddr(vm_t *vm, uintptr_t paddr,
                                             vm_memory_reservation_t *reservation);

/**
 * Map a guest reservation backed with untyped frames
 * @param[in] vm                    A handle to the VM
 * @param[in] reservation           Pointer to reservation object being mapped
 * @return                          -1 on failure otherwise 0 for success
 */
int map_ut_alloc_reservation(vm_t *vm, vm_memory_reservation_t *reservation);

/**
 * Map a guest reservation backed with free vka frames
 * @param[in] vm                    A handle to the VM
 * @param[in] reservation           Pointer to reservation object being mapped
 * @return                          -1 on failure otherwise 0 for success
 */
int map_frame_alloc_reservation(vm_t *vm, vm_memory_reservation_t *reservation);

/**
 * Map a guest reservation backed with device frames
 * @param[in] vm                    A handle to the VM
 * @param[in] reservation           Pointer to reservation object being mapped
 * @return                          -1 on failure otherwise 0 for success
 */
int map_maybe_device_reservation(vm_t *vm, vm_memory_reservation_t *reservation);
