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

#include <simple/simple.h>
#include <vka/vka.h>
#include <vspace/vspace.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/boot_arch.h>

/* ID of the boot vcpu in a VM */
#define BOOT_VCPU 0

/* Initialise/Create VM */
int vm_init(vm_t *vm, vka_t *vka, simple_t *host_simple, allocman_t *allocman, vspace_t host_vspace, vm_plat_callbacks_t callbacks, int priority,
        ps_io_ops_t* io_ops, const char *name, void *cookie);
int vm_init_arch(vm_t *vm, void *cookie);

/* Create a VPCU for a given VM */
int vm_create_vcpu(vm_t *vm, void *cookie, vm_vcpu_t **vcpu, unsigned int vcpu_id);
int vm_create_vcpu_arch(vm_t *vm, void *cookie, vm_vcpu_t *vpcu);
