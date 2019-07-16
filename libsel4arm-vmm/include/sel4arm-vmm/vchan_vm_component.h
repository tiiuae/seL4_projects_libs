/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#pragma once

#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT

#include <sel4arm-vmm/vm.h>
#include <sel4vchan/vchan_component.h>

int reg_new_vchan_con(vm_t *vmm, camkes_vchan_con_t *con);
camkes_vchan_con_t *get_vchan_con(vm_t *vmm, int con_dom_num);

#endif //CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
