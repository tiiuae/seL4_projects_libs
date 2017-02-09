/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "../../../vm.h"

extern const struct device dev_vgic_dist;
extern const struct device dev_vgic_vcpu;
extern const struct device dev_vgic_cpu;


int handle_vgic_maintenance(vm_t* vm, int idx);
