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

#include <sel4vm/sel4_arch/vm.h>

#include <sel4utils/process.h>
#include <platsupport/io.h>

#include <elf/elf.h>
#include <simple/simple.h>
#include <vspace/vspace.h>
#include <vka/vka.h>

#include <sel4vm/atags.h>
#include <sel4vm/devices.h>
#include <sel4vm/fault.h>

#include <sel4vmmcore/util/io.h>
#include <sel4pci/pci.h>

#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
#include <sel4vm/vchan_vm_component.h>
#include <sel4vchan/vchan_component.h>
#endif //CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT

typedef struct vm vm_t;
typedef int (*reboot_hook_fn)(vm_t* vm, void *token);

typedef struct virq_handle *virq_handle_t;

/**
 * Copy out an atag list to the VM.
 * @param[in] vm        A handle to the VM that the ELF should be loaded into
 * @param[in] atag_list A handle to a list of atags
 * @param[in] addr      The address that the atags should be copied to
 * @return              0 on success
 */
int vm_copyout_atags(vm_t *vm, struct atag_list *atags, uint32_t addr);

/**
 * Set the boot args and pc for the VM.
 * For linux:
 *   r0 -> 0
 *   r1 -> MACH_TYPE  (#4151 for EXYNOS5410 eval. platform smdk5410)
 *   r2 -> atags address
 * @param[in] vm        A handle to a VM
 * @param[in] pc        The initial PC for the VM
 * @param[in] mach_type Linux specific machine ID
 *                      see http://www.arm.linux.org.uk/developer/machines/
 * @param[in] atags     Linux specific IPA of atags
 * @return              0 on success
 */
int vm_set_bootargs(vm_t *vm, seL4_Word pc, seL4_Word mach_type, seL4_Word atags);


/**
 * Start up a VM
 * @param[in] vm  The virtual machine to boot
 * @return        0 on success
 */
int vm_start(vm_t *vm);

/**
 * Stop a VM. The VM can be started later with a call to vm_start
 * @param[in] vm The virtual machine to stop
 * @return       0 on success
 */
int vm_stop(vm_t *vm);

/**
 * Handle a VM event
 * @param[in] vm   A handle to the VM that triggered the event
 * @param[in] tag  The tag of the incomming message
 * @return     0 on success, otherwise, the VM should be shut down
 */
int vm_event(vm_t *vm, seL4_MessageInfo_t tag);

/**
 * Register or replace a virtual IRQ definition
 * @param[in] virq  The IRQ number to inject
 * @param[in] ack   A function to call when the VM ACKs the IRQ
 * @param[in] token A token to pass, unmodified, to the ACK callback function
 */
virq_handle_t vm_virq_new(vm_t *vm, int virq, void (*ack)(void *), void *token);

/**
 * Inject an IRQ into a VM
 * @param[in] virq  A handle to the virtual IRQ
 * @return          0 on success
 */
int vm_inject_IRQ(virq_handle_t virq);

/**
 * Install a service into the VM
 * Effectively a cap copy into the CNode of the VM. This allows the VM to
 * make a seL4 call
 * @param[in] vm       The VM to install the service into
 * @param[in] service  A capability for VM
 * @param[in] index    The index at which to install the cap
 * @param[in] badge    The badge to assign to the cap.
 * @return             0 on success
 */
int vm_install_service(vm_t *vm, seL4_CPtr service, int index, uint32_t badge);

/**
 * Given a guest intermiate physical address, find the actual physical address
 * of the mapping.
 * @param[in] vm       The VM in question
 * @param[in] ipa      The guest physical address in question
 * @param[in] size     The size of the request
 * @return             The physical address which corresponds to the provided
 *                     guest physical address or 0 on error. An error will occur
 *                     If a valid mapping is not present, or if the mapping does
 *                     not provide a contiguous region of the requested size.
 */
uintptr_t vm_ipa_to_pa(vm_t *vm, uintptr_t ipa, size_t size);

/**
 * Add a function to be called when the vm is reset by the vmm.
 * @param  vm    The VM
 * @param  hook  reboot_hook_fn to be called when vm resets
 * @param  token token to be passed to hook function
 * @return       0 if callback is successfully registerd.  -1 otherwise.
 */
int vm_register_reboot_callback(vm_t *vm, reboot_hook_fn hook, void *token);

/**
 * Calls all reboot callbacks.  Should be called when VM is rebooted by vmm.
 * @param  vm The VM
 * @return    0 if all callbacks successfully called. If a callback fails with
 *            an error then this returns with that error and callbacks registered
 *            after failing callback will not be called.
 */
int vm_process_reboot_callbacks(vm_t *vm);
