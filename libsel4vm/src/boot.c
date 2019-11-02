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

#include <autoconf.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/util.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/boot.h>
#include <sel4vm/guest_vm_exits.h>

#include "vm_boot.h"

static int curr_vcpu_index = 0;

int
vm_init(vm_t *vm, vka_t *vka, simple_t *host_simple, allocman_t *allocman, vspace_t host_vspace,
        ps_io_ops_t* io_ops, seL4_CPtr host_endpoint, const char* name) {
    int err;
    bzero(vm, sizeof(vm_t));
    /* Initialise vm fields */
    vm->vka = vka;
    vm->simple = host_simple;
    vm->allocman = allocman;
    vm->io_ops = io_ops;
    vm->mem.vmm_vspace = host_vspace;
    vm->host_endpoint = host_endpoint;
    vm->vm_name = strndup(name, strlen(name));
    vm->run.exit_reason = VM_GUEST_UNKNOWN_EXIT;
    /* Initialise ram region */
    vm->mem.num_ram_regions = 0;
    vm->mem.ram_regions = malloc(0);
    /* Currently set this to 4k pages by default */
    vm->mem.page_size = seL4_PageBits;
    /* Initialise our vcpu set */
    vm->vcpus = malloc(sizeof(vm_vcpu_t *) * MAX_NUM_VCPUS);
    assert(vm->vcpus);
    memset(vm->vcpus, 0, sizeof(vm_vcpu_t *) * MAX_NUM_VCPUS);
    /* Initialise vm memory management interface */
    err = vm_memory_init(vm);
    if (err) {
        ZF_LOGE("Failed to initialise VM memory manager");
        return err;
    }

    /* Initialise vm architecture support */
    err = vm_init_arch(vm);
    if (err) {
        ZF_LOGE("Failed to initialise VM architecture support");
        return err;
    }

    /* Flag that the vm has been initialised */
    vm->vm_initialised = true;
    return 0;
}

vm_vcpu_t*
vm_create_vcpu(vm_t *vm, int priority, void *cookie) {
    int err;
    if( vm->num_vcpus >= MAX_NUM_VCPUS) {
        ZF_LOGE("Failed to create vcpu, reached maximum number of support vcpus");
        return NULL;
    }
    vm_vcpu_t *vcpu_new = malloc(sizeof(vm_vcpu_t));
    assert(vcpu_new);
    bzero(vcpu_new, sizeof(vm_vcpu_t));
    /* Create VCPU */
    err = vka_alloc_vcpu(vm->vka, &vcpu_new->vcpu);
    assert(!err);
    /* Initialise vcpu fields */
    vcpu_new->vm = vm;
    vcpu_new->vcpu_id = curr_vcpu_index++;
    vcpu_new->tcb.priority = priority;
    err = vm_create_vcpu_arch(vm, cookie, vcpu_new);
    assert(!err);
    vm->vcpus[vm->num_vcpus] = vcpu_new;
    vm->num_vcpus++;
    return vcpu_new;
}
