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

typedef struct vmm vmm_t;
typedef struct vmm_vcpu vmm_vcpu_t;

#include "sel4vm/platform/vmexit.h"
#include "sel4vm/driver/pci.h"
#include "sel4vm/io.h"
#include "sel4vm/platform/guest_memory.h"
#include "sel4vm/guest_state.h"
#include "sel4vm/vmexit.h"
#include "sel4vm/mmio.h"
#include "sel4vm/processor/lapic.h"
#include "sel4vm/vmcall.h"
#include "sel4vm/vmm_manager.h"

/* Finalize the VM before running it */
int vmm_finalize(vmm_t *vmm);

/*running vmm moudle*/
void vmm_run(vmm_t *vmm);

/* TODO htf did these get here? lets refactor everything  */
void vmm_sync_guest_state(vmm_vcpu_t *vcpu);
void vmm_sync_guest_context(vmm_vcpu_t *vcpu);
void vmm_reply_vm_exit(vmm_vcpu_t *vcpu);

/* mint a badged copy of the vmm's async event notification cap */
seL4_CPtr vmm_create_async_event_notification_cap(vmm_t *vmm, seL4_Word badge);

