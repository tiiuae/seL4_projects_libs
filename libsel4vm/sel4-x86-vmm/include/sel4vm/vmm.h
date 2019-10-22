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

/*This file contains structure definitions and function prototype related with VMM.*/

#pragma once

#include <sel4/sel4.h>

#include <vka/vka.h>
#include <simple/simple.h>
#include <vspace/vspace.h>
#include <allocman/allocman.h>

typedef struct vm vm_t;
typedef struct vm_vcpu vm_vcpu_t;

#include "sel4vm/platform/vmexit.h"
#include "sel4vm/ioports.h"
#include "sel4vm/guest_memory.h"
#include "sel4vm/vmexit.h"
#include "sel4vm/processor/lapic.h"
#include "sel4vm/vmm_manager.h"
