/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#ifndef __VCHAN_VM_COMPONENT
#define __VCHAN_VM_COMPONENT

#include <sel4arm-vmm/vm.h>
#include <sel4vchan/vchan_component.h>

int reg_new_vchan_con(vm_t *vmm, camkes_vchan_con_t *con);
camkes_vchan_con_t *get_vchan_con(vm_t *vmm, int con_dom_num);

#endif

