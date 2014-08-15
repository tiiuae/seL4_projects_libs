/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef SEL4ARM_VMM_VM_H
#define SEL4ARM_VMM_VM_H

#include <sel4utils/process.h>
#include <platsupport/io.h>

#include <simple/simple.h>
#include <vspace/vspace.h>
#include <vka/vka.h>

#include <sel4arm-vmm/atags.h>
#include <sel4arm-vmm/devices.h>
#include <sel4arm-vmm/fault.h>

#define MAX_DEVICES_PER_VM 50

struct vm {
    /* Identification */
    const char* name;
    int vmid;
    /* OS support */
    vka_t *vka;
    simple_t *simple;
    vspace_t *vmm_vspace;
    ps_io_ops_t* io_ops;
    struct irq_sys* irq_sys;
    /* VM objects */
    vspace_t vm_vspace;
    sel4utils_alloc_data_t data;
    vka_object_t cspace;
    vka_object_t tcb;
    vka_object_t pd;
    vka_object_t vcpu;
    seL4_CPtr fault_ep;
    /* Installed devices */
    struct device devices[MAX_DEVICES_PER_VM];
    int ndevices;
    /* Other */
    void *entry_point;
    struct vm_onode* onode_head;
    /* Fault structure */
    fault_t fault;
};
typedef struct vm vm_t;

/**
 * Create a virtual machine
 * @param name           A logical name for the VM
 * @param priority       The priority of the new VM
 * @param vmm_endpoint   An endpoint for IPC delivery such as VM faults
 * @param vm_badge       The badge for delivered IPCs
 * @param vka            A vka for object allocation.
 *                       Must be valid for the life of the VM.
 * @param simple         A simple instance for key cap acquisition.
 *                       Must be valid for the life of the VM.
 * @param vmm_vspace     The vspace of the VMM.
 *                       Must be valid for the life of the VM.
 * @param io_ops         IO operations for the VM to use when mapping and
 *                       configuring hardware.
 *                       Must be valid for the life of the VM.
 * @param vm             A reference to a vm struct to initialise
 * @return               0 on success
 */
int vm_create(const char* name, int priority,
              seL4_CPtr vmm_endpoint, seL4_Word vm_badge,
              vka_t *vka, simple_t *simple, vspace_t *vspace,
              ps_io_ops_t* io_ops,
              vm_t* vm);

/**
 * Copy data out to the VM.
 * @param[in] vm      A handle to the VM that the data should be loaded into
 * @param[in] data    The address of the data to load
 * @param[in] address The VM IPA address that the data should be loaded at
 * @param[in] size    The number of bytes of data to load
 * @return            0 on success
 */
int vm_copyout(vm_t* vm, void* data, uintptr_t address, size_t size);

/**
 * Copy ELF segments out to the VM/
 * The virtual addresses in the ELF file are assummed to represent IPA addresses
 * @param[in] vm       A handle to the VM that the ELF should be loaded into
 * @param[in] elf_data The address of the ELF file header
 * @return             On success, returns the IPA entry point of the ELF file
 */
void* vm_copyout_elf(vm_t* vm, void* elf_data);

/**
 * Copy out an atag list to the VM.
 * @param[in] vm        A handle to the VM that the ELF should be loaded into
 * @param[in] atag_list A handle to a list of atags
 * @param[in] addr      The address that the atags should be copied to
 * @return              0 on success
 */
int vm_copyout_atags(vm_t* vm, struct atag_list* atags, uint32_t addr);

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
int vm_set_bootargs(vm_t* vm, void* pc, uint32_t mach_type, uint32_t atags);


/**
 * Start up a VM
 * @param[in] vm  The virtual machine to boot
 * @return        0 on success
 */
int vm_start(vm_t* vm);

/**
 * Stop a VM. The VM can be started later with a call to vm_start
 * @param[in] vm The virtual machine to stop
 * @return       0 on success
 */
int vm_stop(vm_t* vm);

/**
 * Loop waiting for a VM event
 * @param vm   A handle to the VM that triggered the event
 * @param tag  The tag of the incomming message
 * @return     0 on success, otherwise, the VM should be shut down
 */
int vm_event(vm_t* vm, seL4_MessageInfo_t tag);





#endif /* SEL4ARM_VMM_VM_H */
