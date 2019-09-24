/*
 * Copyright 2017, Data61
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

#include <stdint.h>

#include <sel4vm/vm.h>
#include <sel4vm/fault.h>
#include <platsupport/gpio.h>
#include <platsupport/plat/clock.h>

typedef struct vm vm_t;

/**
 * Map a given frame cap into a VM's IPA.
 * @param[in] vm         A handle to the VM that the frame should be mapped into
 * @param[in] cap        Cap of the frame to be mapped
 * @param[in] ipa        The IPA that the frame is to be mapped at
 * @param[in] size_bits  The frame size in bits
 * @param[in] cached     Is it mapped with caching enabled?
 * @param[in] vm_rights  Mapping rights
 * @return               0 on success
 */
int vm_map_frame(vm_t *vm, seL4_CPtr cap, uintptr_t ipa, size_t size_bits, int cached, seL4_CapRights_t vm_rights);
