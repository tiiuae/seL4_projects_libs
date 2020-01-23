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

#include <sel4/sel4.h>

#include <vka/vka.h>
#include <simple/simple.h>
#include <vspace/vspace.h>
#include <allocman/allocman.h>

#include <sel4vm/arch/guest_vm_arch.h>
#include <sel4vm/guest_memory.h>

typedef struct vm vm_t;
typedef struct vm_vcpu vm_vcpu_t;
typedef struct vm_mem vm_mem_t;
typedef struct vm_ram_region vm_ram_region_t;
typedef struct vm_run vm_run_t;
typedef struct vm_arch vm_arch_t;

typedef memory_fault_result_t (*unhandled_mem_fault_callback_fn)(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t paddr,
                                                                 size_t len, void *cookie);
typedef int (*notification_callback_fn)(vm_t *vm, seL4_Word badge, seL4_MessageInfo_t tag,
                                        void *cookie);

struct vm_ram_region {
    /* Guest physical start address */
    uintptr_t start;
    /* size in bytes */
    size_t size;
    /* whether or not this region has been 'allocated' */
    int allocated;
};

struct vm_mem {
    /* Guest vm vspace management */
    vspace_t vm_vspace;
    /* Guest vm root vspace */
    vka_object_t vm_vspace_root;
    /* vmm vspace */
    vspace_t vmm_vspace;
    /* Guest vm ram regions */
    /* We maintain all pieces of ram as a sorted list of regions.
     * This is memory that we will specifically give the guest as actual RAM */
    int num_ram_regions;
    struct vm_ram_region *ram_regions;
    /* Memory reservations */
    vm_memory_reservation_cookie_t *reservation_cookie;
    unhandled_mem_fault_callback_fn unhandled_mem_fault_handler;
    void *unhandled_mem_fault_cookie;
};

struct vm_tcb {
    /* Guest vm tcb management objects */
    vka_object_t tcb;
    vka_object_t sc;
    vka_object_t sched_ctrl;
    /* Guest vm scheduling priority */
    int priority;
};

struct vm_vcpu {
    /* Parent vm */
    struct vm *vm;
    /* Kernel vcpu object */
    vka_object_t vcpu;
    /* vm tcb */
    struct vm_tcb tcb;
    /* Id of vcpu */
    unsigned int vcpu_id;
    /* The identifier used by the guest to enable this vcpu */
    int target_cpu;
    /* is the vcpu online */
    bool vcpu_online;
    /* Architecture specfic vcpu */
    struct vm_vcpu_arch vcpu_arch;
};

struct vm_run {
    /* Records last vm exit reason */
    int exit_reason;
    notification_callback_fn notification_callback;
    void *notification_callback_cookie;
};

struct vm_cspace {
    /* Kernel cspace object */
    vka_object_t cspace_obj;
    seL4_Word cspace_root_data;
};

struct vm {
    /* Architecture specfic vm structure */
    struct vm_arch arch;
    /* vm vcpus */
    unsigned int num_vcpus;
    struct vm_vcpu *vcpus[CONFIG_MAX_NUM_NODES];
    /* vm memory management */
    struct vm_mem mem;
    /* vm runtime management */
    struct vm_run run;
    /* Guest vm cspace */
    struct vm_cspace cspace;
    /* Host endoint (i.e. vmm) to wait for VM faults and host events */
    seL4_CPtr host_endpoint;
    /* Support & Resource Managements */
    vka_t *vka;
    ps_io_ops_t *io_ops;
    simple_t *simple;
    allocman_t *allocman;
    /* Debugging & Identification */
    char *vm_name;
    unsigned int vm_id;
    bool vm_initialised;
};

/* Run the VM */
int vm_run(vm_t *vm);
/* Start a vcpu */
int vcpu_start(vm_vcpu_t *vcpu);

/* Unhandled fault callback registration functions */
int vm_register_unhandled_mem_fault_callback(vm_t *vm, unhandled_mem_fault_callback_fn fault_handler,
                                             void *cookie);
int vm_register_notification_callback(vm_t *vm, notification_callback_fn notification_callback,
                                      void *cookie);
