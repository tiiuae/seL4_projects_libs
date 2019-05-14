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
#include <autoconf.h>
#include <sel4vm/gen_config.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/boot.h>
#include <sel4vm/guest_vm_util.h>
#include <sel4vm/guest_memory.h>

#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

#include <cpio/cpio.h>
#include <vka/object.h>
#include <vka/capops.h>
#include <string.h>
#include <sel4utils/mapping.h>
#include <utils/ansi.h>

#include <sel4/sel4.h>
#include <sel4/messages.h>

#include "devices.h"

#include <sel4vm/sel4_arch/vm.h>
#include <sel4vmmcore/util/io.h>

//#define DEBUG_VM
//#define DEBUG_RAM_FAULTS
//#define DEBUG_DEV_FAULTS
//#define DEBUG_STRACE

#define VM_CSPACE_SIZE_BITS    4
#define VM_FAULT_EP_SLOT       1
#define VM_CSPACE_SLOT         2

#ifdef DEBUG_RAM_FAULTS
#define DRAMFAULT(...) printf(__VA_ARGS__)
#else
#define DRAMFAULT(...) do{}while(0)
#endif

#ifdef DEBUG_DEV_FAULTS
#define DDEVFAULT(...) printf(__VA_ARGS__)
#else
#define DDEVFAULT(...) do{}while(0)
#endif

#ifdef DEBUG_STRACE
#define DSTRACE(...) printf(__VA_ARGS__)
#else
#define DSTRACE(...) do{}while(0)
#endif

#ifdef DEBUG_VM
#define DVM(...) do{ printf(__VA_ARGS__);}while(0)
#else
#define DVM(...) do{}while(0)
#endif

extern char _cpio_archive[];

static int alloc_vm_device_cap(uintptr_t addr, vm_t* vm, vm_frame_t *frame_result) {
    int err;
    cspacepath_t frame;
    err = vka_cspace_alloc_path(vm->vka, &frame);
    if (err) {
        ZF_LOGE("Failed to allocate cslot\n");
        return -1;
    }
    seL4_Word cookie;
    err = vka_utspace_alloc_at(vm->vka, &frame, kobject_get_type(KOBJECT_FRAME, 12), 12, addr, &cookie);
    if (err) {
        err = simple_get_frame_cap(vm->simple, (void*)addr, 12, &frame);
        if (err) {
            return -1;
        }
    }
    frame_result->cptr = frame.capPtr;
    frame_result->rights = seL4_AllRights;
    frame_result->vaddr = addr;
    frame_result->size_bits = vm->mem.page_size;
    return 0;
}

static int alloc_vm_ram_cap(uintptr_t addr, vm_t* vm, vm_frame_t *frame_result) {
    int err;
    cspacepath_t frame;
    vka_object_t frame_obj;
    err = vka_alloc_frame_maybe_device(vm->vka, 12, true, &frame_obj);
    if (err) {
        ZF_LOGF("Failed vka_alloc_frame_maybe_device");
        return -1;
    }
    vka_cspace_make_path(vm->vka, frame_obj.cptr, &frame);
    frame_result->cptr = frame.capPtr;
    frame_result->rights = seL4_AllRights;
    frame_result->vaddr = addr;
    frame_result->size_bits = vm->mem.page_size;
    return 0;
}

static vm_frame_t on_demand_iterator(uintptr_t addr, void *cookie) {
    int err;
    uintptr_t paddr = addr & ~0xfff;
    vm_frame_t frame_result = { seL4_CapNull, seL4_NoRights, 0, 0 };
    vm_t *vm = (vm_t *)cookie;
    /* Attempt allocating device memory */
    err = alloc_vm_device_cap(paddr, vm, &frame_result);
    if (!err) {
        return frame_result;
    }
    /* Attempt allocating ram memory */
    err = alloc_vm_ram_cap(paddr, vm, &frame_result);
    if (err) {
        ZF_LOGE("Failed to create on demand memory for addr 0x%x", addr);
    }
    return frame_result;
}

static memory_fault_result_t
handle_on_demand_fault_callback(vm_t *vm, uintptr_t fault_addr, size_t fault_length,
        void *cookie, guest_memory_arch_data_t arch_data) {
    ZF_LOGE("Fault for on demand memory region: 0x%x", fault_addr);
    return FAULT_ERROR;
}

static int
handle_page_fault(vm_t* vm, fault_t* fault)
{
    struct device* d;
    guest_memory_arch_data_t arch_data;
    uintptr_t addr = fault_get_address(fault);
    size_t fault_size = fault_get_width_size(fault);

    arch_data.fault = fault;
    memory_fault_result_t fault_result = vm_memory_handle_fault(vm, addr, fault_size, arch_data);
    switch(fault_result) {
        case FAULT_HANDLED:
            return 0;
        case FAULT_RESTART:
            restart_fault(fault);
            return 0;
        case FAULT_IGNORE:
            return ignore_fault(fault);
        case FAULT_ERROR:
            print_fault(fault);
            abandon_fault(fault);
            return -1;
        default: /* FAULT_UNHANDLED */
            break;
            /* We don't have a memory reservation for the faulting address
             * We move onto the rest of the page fault handler */
    }

    /* See if the device is already in our address space */
    d = vm_find_device_by_ipa(vm, fault_get_address(fault));
    if (d != NULL) {
        if (d->devid == DEV_RAM) {
            DRAMFAULT("[%s] %s fault @ 0x%x from 0x%x\n", d->name,
                      (fault_is_read(fault)) ? "read" : "write",
                      fault_get_address(fault), fault_get_ctx(fault)->pc);
        } else {
            DDEVFAULT("[%s] %s fault @ 0x%x from 0x%x\n", d->name,
                      (fault_is_read(fault)) ? "read" : "write",
                      fault_get_address(fault), fault_get_ctx(fault)->pc);
        }
        return d->handle_page_fault(d, vm, fault);
    } else {
#ifdef CONFIG_ONDEMAND_DEVICE_INSTALL
        uintptr_t addr = fault_get_address(fault) & ~0xfff;
        int mapped;
        vm_memory_reservation_t *reservation;
        switch (addr) {
        case 0:
            print_fault(fault);
            return -1;
        default:
            reservation = vm_reserve_memory_at(vm, addr, 0x1000,
                    handle_on_demand_fault_callback, NULL);
            mapped = vm_map_reservation(vm, reservation, on_demand_iterator, (void *)vm);
            if (!mapped) {
                restart_fault(fault);
                return 0;
            }
            DVM("Unhandled fault on address 0x%x\n", (uint32_t)addr);
        }
#endif
        print_fault(fault);
        abandon_fault(fault);
        return -1;
    }
}

static int handle_exception(vm_t *vm, seL4_Word ip)
{
    seL4_UserContext regs;
    seL4_CPtr tcb = vm_get_tcb(vm);
    int err;
    printf("%sInvalid instruction from [%s] at PC: 0x"XFMT"%s\n",
           ANSI_COLOR(RED, BOLD), vm->vm_name, seL4_GetMR(0), ANSI_COLOR(RESET));
    err = seL4_TCB_ReadRegisters(tcb, false, 0, sizeof(regs) / sizeof(regs.pc), &regs);
    assert(!err);
    print_ctx_regs(&regs);
    return 1;
}

int vm_set_bootargs(vm_t *vm, seL4_Word pc, seL4_Word mach_type, seL4_Word atags)
{
    seL4_UserContext regs;
    seL4_CPtr tcb;
    int err;
    assert(vm);
    /* Write CPU registers */
    tcb = vm_get_tcb(vm);
    err = seL4_TCB_ReadRegisters(tcb, false, 0, sizeof(regs) / sizeof(regs.pc), &regs);
    assert(!err);
    sel4arch_set_bootargs(&regs, pc, mach_type, atags);
    err = seL4_TCB_WriteRegisters(tcb, false, 0, sizeof(regs) / sizeof(regs.pc), &regs);
    assert(!err);
    return err;
}

static void
sys_pa_to_ipa(vm_t* vm, seL4_UserContext* regs)
{
    uint32_t pa;
#ifdef CONFIG_ARCH_AARCH64
#else
    pa = regs->r0;
#endif

    DSTRACE("PA translation syscall from [%s]: 0x%08x->?\n", vm->vm_name, pa);
#ifdef CONFIG_ARCH_AARCH64
#else
    regs->r0 = pa;
#endif
}


#if 0
/* sys_ipa_to_pa currently not supported
 * TODO: Re-enable or re-evaluate support for syscall
 */
static void
sys_ipa_to_pa(vm_t* vm, seL4_UserContext* regs)
{
    seL4_ARM_Page_GetAddress_t ret;
    long ipa;
    seL4_CPtr cap;
#ifdef CONFIG_ARCH_AARCH64
#else
    ipa = regs->r0;
#endif
    cap = vspace_get_cap(vm_get_vspace(vm), (void *)ipa);
    if (cap == seL4_CapNull) {
        void *mapped_address;
        mapped_address = map_vm_ram(vm, ipa);
        if (mapped_address == NULL) {
            printf("Could not map address for IPA translation\n");
            return;
        }
        cap = vspace_get_cap(vm_get_vspace(vm), (void *)ipa);
        assert(cap != seL4_CapNull);
    }

    ret = seL4_ARM_Page_GetAddress(cap);
    assert(!ret.error);
    DSTRACE("IPA translation syscall from [%s]: 0x%08x->0x%08x\n",
            vm->vm_name, ipa, ret.paddr);
#ifdef CONFIG_ARCH_AARCH64
#else
    regs->r0 = ret.paddr;
#endif
}
#endif

static void sys_nop(vm_t *vm, seL4_UserContext *regs)
{
    DSTRACE("NOP syscall from [%s]\n", vm->vm_name);
}

static int handle_syscall(vm_t *vm, seL4_Word length)
{
    seL4_Word syscall, ip;
    seL4_UserContext regs;
    seL4_CPtr tcb;
    int err;

    syscall = seL4_GetMR(seL4_UnknownSyscall_Syscall);
    ip = seL4_GetMR(seL4_UnknownSyscall_FaultIP);

    tcb = vm_get_tcb(vm);
    err = seL4_TCB_ReadRegisters(tcb, false, 0, sizeof(regs) / sizeof(regs.pc), &regs);
    assert(!err);
    regs.pc += 4;

    DSTRACE("Syscall %d from [%s]\n", syscall, vm->vm_name);
    switch (syscall) {
    case 65:
        sys_pa_to_ipa(vm, &regs);
        break;
#if 0
    case 66:
        sys_ipa_to_pa(vm, &regs);
        break;
#endif
    case 67:
        sys_nop(vm, &regs);
        break;
    default:
        printf("%sBad syscall from [%s]: scno %zd at PC: %p%s\n",
               ANSI_COLOR(RED, BOLD), vm->vm_name, syscall, (void *) ip, ANSI_COLOR(RESET));
        return -1;
    }
    err = seL4_TCB_WriteRegisters(tcb, false, 0, sizeof(regs) / sizeof(regs.pc), &regs);
    assert(!err);
    return 0;
}

int vm_event(vm_t *vm, seL4_MessageInfo_t tag)
{
    seL4_Word label;
    seL4_Word length;

    label = seL4_MessageInfo_get_label(tag);
    length = seL4_MessageInfo_get_length(tag);

    switch (label) {
    case seL4_Fault_VMFault: {
        int err;
        fault_t* fault;
        fault = vm->vcpus[BOOT_VCPU]->vcpu_arch.fault;
        err = new_fault(fault);
        assert(!err);
        do {
            err = handle_page_fault(vm, fault);
            if (err) {
                return -1;
            }
        } while (!fault_handled(fault));
    }
    break;

    case seL4_Fault_UnknownSyscall: {
        int err;
        assert(length == seL4_UnknownSyscall_Length);
        err = handle_syscall(vm, length);
        assert(!err);
        if (!err) {
            seL4_MessageInfo_t reply;
            reply = seL4_MessageInfo_new(0, 0, 0, 0);
            seL4_Reply(reply);
        }
    }
    break;

    case seL4_Fault_UserException: {
        seL4_Word ip;
        int err;
        assert(length == seL4_UserException_Length);
        ip = seL4_GetMR(0);
        err = handle_exception(vm, ip);
        assert(!err);
        if (!err) {
            seL4_MessageInfo_t reply;

            reply = seL4_MessageInfo_new(0, 0, 0, 0);
            seL4_Reply(reply);
        }
    }
    break;
    case seL4_Fault_VGICMaintenance: {
        int idx;
        int err;
        assert(length == seL4_VGICMaintenance_Length);
        idx = seL4_GetMR(seL4_UnknownSyscall_ARG0);
        /* Currently not handling spurious IRQs */
        assert(idx >= 0);

        err = handle_vgic_maintenance(vm, idx);
        assert(!err);
        if (!err) {
            seL4_MessageInfo_t reply;

            reply = seL4_MessageInfo_new(0, 0, 0, 0);
            seL4_Reply(reply);
        }
    }
    break;
    case seL4_Fault_VCPUFault: {
        seL4_MessageInfo_t reply;
        uint32_t hsr;
        int err;
        fault_t* fault;
        fault = vm->vcpus[BOOT_VCPU]->vcpu_arch.fault;
        assert(length == seL4_VCPUFault_Length);
        hsr = seL4_GetMR(seL4_UnknownSyscall_ARG0);
        /* check if the exception class (bits 26-31) of the HSR indicate WFI/WFE */
        if ((hsr >> 26) == 1) {
            /* generate a new WFI fault */
            new_wfi_fault(fault);
            return 0;
        } else {
            printf("Unhandled VCPU fault from [%s]: HSR 0x%08x\n", vm->vm_name, hsr);
            if ((hsr & 0xfc300000) == 0x60200000 || hsr == 0xf2000800) {
                seL4_UserContext *regs;
                new_wfi_fault(fault);
                regs = fault_get_ctx(fault);
                regs->pc += 4;
                seL4_TCB_WriteRegisters(vm_get_tcb(vm), false, 0,
                                        sizeof(*regs) / sizeof(regs->pc), regs);
                restart_fault(fault);
                return 0;
            }
            return -1;
        }
    }
    break;
    default:
        /* What? Why are we here? What just happened? */
        printf("Unknown fault from [%s]: label=%p length=%p\n",
               vm->vm_name, (void *) label, (void *) length);
        return -1;
    }
    return 0;
}

int vm_copyout_atags(vm_t *vm, struct atag_list *atags, uint32_t addr)
{
    vspace_t *vm_vspace, *vmm_vspace;
    void *vm_addr, *vmm_addr, *buf;
    reservation_t res;
    vka_t *vka;
    vka_object_t frame;
    size_t size;
    struct atag_list *atag_cur;
    int err;

    vka = vm->vka;
    vm_addr = (void *)(addr & ~0xffflu);
    vm_vspace = vm_get_vspace(vm);
    vmm_vspace = vm_get_vmm_vspace(vm);

    /* Make sure we don't cross a page boundary
     * NOTE: the next page will usually be used by linux for PT!
     */
    for (size = 0, atag_cur = atags; atag_cur != NULL; atag_cur = atag_cur->next) {
        size += atags_size_bytes(atag_cur);
    }
    size += 8; /* NULL tag */
    assert((addr & 0xfff) + size < 0x1000);

    /* Create a frame (and a copy for the VMM) */
    err = vka_alloc_frame(vka, 12, &frame);
    assert(!err);
    if (err) {
        return -1;
    }
    /* Map the frame to the VMM */
    vmm_addr = vspace_map_pages(vmm_vspace, &frame.cptr, NULL, seL4_AllRights, 1, 12, 0);
    assert(vmm_addr);

    /* Copy in the atags */
    buf = vmm_addr + (addr & 0xfff);
    for (atag_cur = atags; atag_cur != NULL; atag_cur = atag_cur->next) {
        int tag_size = atags_size_bytes(atag_cur);
        DVM("ATAG copy 0x%x<-0x%x %d\n", (uint32_t)buf, (uint32_t)atag_cur->hdr, tag_size);
        memcpy(buf, atag_cur->hdr, tag_size);
        buf += tag_size;
    }
    /* NULL tag terminator */
    memset(buf, 0, 8);

    /* Unmap the page and map it into the VM */
    vspace_unmap_pages(vmm_vspace, vmm_addr, 1, 12, NULL);
    res = vspace_reserve_range_at(vm_vspace, vm_addr, 0x1000, seL4_AllRights, 0);
    assert(res.res);
    if (!res.res) {
        vka_free_object(vka, &frame);
        return -1;
    }
    err = vspace_map_pages_at_vaddr(vm_vspace, &frame.cptr, NULL, vm_addr, 1, 12, res);
    vspace_free_reservation(vm_vspace, res);
    assert(!err);
    if (err) {
        printf("Failed to provide memory\n");
        vka_free_object(vka, &frame);
        return -1;
    }

    return 0;
}

int vm_add_device(vm_t *vm, const struct device *d)
{
    assert(d != NULL);
    if (vm->arch.ndevices < MAX_DEVICES_PER_VM) {
        vm->arch.devices[vm->arch.ndevices++] = *d;
        return 0;
    } else {
        return -1;
    }
}

static int cmp_id(struct device *d, void *data)
{
    return !(d->devid == *((enum devid *)data));
}

static int cmp_ipa(struct device *d, void *data)
{
    return !dev_paddr_in_range(*(uintptr_t *)data, d);
}

struct device *
vm_find_device(vm_t *vm, int (*cmp)(struct device *d, void *data), void *data)
{
    struct device *ret;
    int i;
    for (i = 0, ret = vm->arch.devices; i < vm->arch.ndevices; i++, ret++) {
        if (cmp(ret, data) == 0) {
            return ret;
        }
    }
    return NULL;
}

struct device *
vm_find_device_by_id(vm_t *vm, enum devid id)
{
    return vm_find_device(vm, &cmp_id, &id);
}

struct device *
vm_find_device_by_ipa(vm_t *vm, uintptr_t ipa)
{
    return vm_find_device(vm, &cmp_ipa, &ipa);
}

int
vm_install_service(vm_t* vm, seL4_CPtr service, int index, uint32_t b)
{
    cspacepath_t src, dst;
    seL4_Word badge = b;
    int err;
    vka_cspace_make_path(vm->vka, service, &src);
    dst.root = vm->tcb.cspace.cptr;
    dst.capPtr = index;
    dst.capDepth = VM_CSPACE_SIZE_BITS;
    err =  vka_cnode_mint(&dst, &src, seL4_AllRights, badge);
    return err;
}

uintptr_t vm_ipa_to_pa(vm_t *vm, uintptr_t ipa_base, size_t size)
{
    seL4_ARM_Page_GetAddress_t ret;
    uintptr_t pa_base = 0;
    uintptr_t ipa;
    vspace_t *vspace;
    vspace = vm_get_vspace(vm);
    ipa = ipa_base;
    do {
        seL4_CPtr cap;
        int bits;
        /* Find the cap */
        cap = vspace_get_cap(vspace, (void *)ipa);
        if (cap == seL4_CapNull) {
            return 0;
        }
        /* Find mapping size */
        bits = vspace_get_cookie(vspace, (void *)ipa);
        assert(bits == 12 || bits == 21);
        /* Find the physical address */
        ret = seL4_ARM_Page_GetAddress(cap);
        if (ret.error) {
            return 0;
        }
        if (ipa == ipa_base) {
            /* Record the result */
            pa_base = ret.paddr + (ipa & MASK(bits));
            /* From here on, ipa and ret.paddr will be aligned */
            ipa &= ~MASK(bits);
        } else {
            /* Check for a contiguous mapping */
            if (ret.paddr - pa_base != ipa - ipa_base) {
                return 0;
            }
        }
        ipa += BIT(bits);
    } while (ipa - ipa_base < size);
    return pa_base;
}

int vm_register_reboot_callback(vm_t *vm, reboot_hook_fn hook, void *token)
{
    if (hook == NULL) {
        ZF_LOGE("hook is NULL");
        return -1;
    }
    struct reboot_hooks rb = {
        .fn = hook,
        .token = token
    };
    if (vm->arch.nhooks < MAX_REBOOT_HOOKS_PER_VM) {
        vm->arch.rb_hooks[vm->arch.nhooks++] = rb;
        return 0;
    } else {
        return -1;
    }

}

int vm_process_reboot_callbacks(vm_t *vm) {
    for (int i = 0; i < vm->arch.nhooks; i++) {
        struct reboot_hooks rb = vm->arch.rb_hooks[i];
        if (rb.fn == NULL) {
            ZF_LOGE("NULL call back has been registered");
            return -1;
        }
        int err = rb.fn(vm, rb.token);
        if (err) {
            ZF_LOGE("Callback returned error: %d", err);
            return err;
        }
    }
    return 0;
}
