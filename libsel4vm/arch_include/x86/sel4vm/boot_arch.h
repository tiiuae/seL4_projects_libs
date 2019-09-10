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

#define MAX_NUM_VCPUS 1

typedef struct vm_init_x86_config vm_init_x86_config_t;

struct vm_init_x86_config {
    seL4_CPtr notification_cap;
    int (*get_interrupt)();
    int (*has_interrupt)();
};
