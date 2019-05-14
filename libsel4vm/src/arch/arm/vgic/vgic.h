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

extern const struct device dev_vgic_dist;
extern const struct device dev_vgic_vcpu;
extern const struct device dev_vgic_cpu;

int vm_vgic_maintenance_handler(vm_vcpu_t *vcpu);
