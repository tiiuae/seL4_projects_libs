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

/* ARM VM Exit Reasons */
#define VM_GUEST_ABORT_EXIT 0
#define VM_SYSCALL_EXIT 1
#define VM_USER_EXCEPTION_EXIT 2
#define VM_VGIC_MAINTENANCE_EXIT 3
#define VM_VCPU_EXIT 4
#define VM_UNKNOWN_EXIT 5

/* Number of VM Exit reasons */
#define VM_NUM_EXIT_HANDLERS 6

/* HSR Constants */
#define HSR_WFI 0x1
#define HSR_CLASS_EXCEPTION_SHIFT 26
