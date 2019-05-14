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

#include <sel4vm/guest_vm_arch.h>
#include <sel4vm/guest_memory.h>

typedef struct vm vm_t;
typedef struct vm_vcpu vm_vcpu_t;
typedef struct vm_mem vm_mem_t;
typedef struct vm_ram_region vm_ram_region_t;
typedef struct vm_run vm_run_t;
typedef struct vm_mmio_range vm_mmio_range_t;
typedef struct vm_arch vm_arch_t;

typedef int (*vm_mmio_fault_fn)(vm_t *vm, vm_mmio_range_t *range, void *cookie,
        uint32_t addr);

typedef struct vm_mmio_range {
    /* MMIO start and end address */
    uintptr_t start;
    size_t size;
    /* Fault handler for mmio region */
    vm_mmio_fault_fn fault_handler;
    /* Private data */
    void *priv;
    /* Debugging & Identification */
    const char *name;
} vm_mmio_range_t;

typedef struct vm_mmio_list {
    /* Sorted array of mmio ranges */
    vm_mmio_range_t *ranges;
    int num_ranges;
} vm_mmio_list_t;

typedef struct vm_plat_callbacks {
    int (*get_interrupt)();
    int (*has_interrupt)();
    int (*do_async)(seL4_Word badge, seL4_Word label);
    seL4_CPtr (*get_async_event_notification)();
} vm_plat_callbacks_t;

typedef struct vm_mmio_abort {
    uintptr_t paddr;
    size_t len;
    bool is_write;
    seL4_Word data;
    seL4_Word data_mask;
    int abort_result;
} vm_mmio_abort_t;

typedef struct vm_io_abort {
    unsigned int port_no;
    bool is_in;
    unsigned int value;
    unsigned int size;
    int abort_result;
} vm_io_abort_t;

struct vm_ram_region {
    /* Guest physical start address */
    uintptr_t start;
    /* size in bytes */
    size_t size;
    /* whether or not this region has been 'allocated' */
    int allocated;
    vm_memory_reservation_t *region_reservation;
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
    /* Default page size to use */
    int page_size;
    /* Memory mapped io management for emulated devices */
    vm_mmio_list_t mmio_list;
    /* Memory reservations */
    vm_memory_reservation_cookie_t *reservation_cookie;
};

struct vm_tcb {
    /* Guest vm tcb management objects */
    vka_object_t tcb;
    vka_object_t sc;
    vka_object_t sched_ctrl;
    /* Guest vm scheduling priority */
    int priority;
    /* Guest vm cspace */
    vka_object_t cspace;
    /* TCB State */
    bool is_suspended;
};

struct vm_vcpu {
    /* Parent vm */
    struct vm *vm;
    /* Kernel vcpu object */
    vka_object_t vcpu;
    /* Id of vcpu */
    unsigned int vcpu_id;
    /* is the vcpu online */
    bool vcpu_online;
    /* Architecture specfic vcpu */
    struct vm_vcpu_arch vcpu_arch;
};

struct vm_run {
    /* Records last vm exit reason */
    int exit_reason;
    /* Records last vm mmio data abort */
    vm_mmio_abort_t mmio_abort;
    vm_io_abort_t io_abort;
};

struct vm {
    /* Architecture specfic vm structure */
    struct vm_arch arch;
    /* vm vcpus */
    unsigned int num_vcpus;
    struct vm_vcpu **vcpus;
    /* vm memory management */
    struct vm_mem mem;
    /* vm tcb */
    struct vm_tcb tcb;
    /* vm runtime management */
    struct vm_run run;
    /* Support & Resource Managements */
    vka_t *vka;
    ps_io_ops_t* io_ops;
    simple_t *simple;
    allocman_t *allocman;
    vm_plat_callbacks_t callbacks;
    /* Debugging & Identification */
    char* vm_name;
    unsigned int vm_id;
    bool vm_initialised;
};
