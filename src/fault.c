/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "fault.h"
#include "vm.h"

#include <sel4/sel4.h>
#include <sel4/messages.h>
#include <vka/capops.h>

#include <stdlib.h>

//#define DEBUG_FAULTS
//#define DEBUG_ERRATA

#ifdef DEBUG_FAULTS
#define DFAULT(...) printf(__VA_ARGS__)
#else
#define DFAULT(...) do{}while(0)
#endif

#ifdef DEBUG_ERRATA
#define DERRATA(...) printf(__VA_ARGS__)
#else
#define DERRATA(...) printf(__VA_ARGS__)
#endif

#ifdef PLAT_EXYNOS5250
#define HAS_ERRATA766422() 1
#else
#define HAS_ERRATA766422() 0
#endif

#define COLOR_ERROR "\033[1;31m"
#define COLOR_NORMAL "\033[0m"

#define CPSR_THUMB         BIT(5)

#define HSR_INST32         BIT(25)
#define HSR_SYNDROME_VALID BIT(24)
#define HSR_SYNDROME_RT(x) (((x) >> 16) & 0xf)

static int
fetch_fault_instruction(fault_t* fault, uint32_t* ret)
{
    int size = ((fault->fsr) & HSR_INST32) ? 4 : 2;
    *ret = 0;
    /* Fetch the ret */
    if (vm_copyin(fault->vm, ret, fault->ip, size) < 0) {
        return -1;
    }
    /* Swap half words for a 32 bit ret */
    if (size == 4) {
        *ret = ((*ret & 0xffff)) << 16 | ((*ret >> 16) & 0xffff);
    }
    return 0;
}

static int
errata766422_get_rt(fault_t* f, uint32_t hsr)
{
    uint32_t instruction;
    int err;
    err = fetch_fault_instruction(f, &instruction);
    if (err) {
        return err;
    }
    if (hsr & HSR_INST32) {
        DERRATA("Errata766422 @ 0x%08x (0x%08x)\n", f->regs.pc, instruction);
        if ((instruction & 0xff700000) == 0xf8400000) {
            return (instruction >> 12) & 0xf;
        } else if ((instruction & 0xfff00000) == 0xf8800000) {
            return (instruction >> 12) & 0xf;
        } else if ((instruction & 0xfff00000) == 0xf0000000) {
            return (instruction >> 12) & 0xf;
        } else if ((instruction & 0x0e500000) == 0x06400000) {
            return (instruction >> 12) & 0xf;
        } else {
            printf("Unable to decode instruction 0x%08x\n", instruction);
            return -1;
        }
    } else {
        printf("Errata766422 @ 0x%08x (0x%04x)\n", f->regs.pc, instruction);
        /* 16 bit instructions */
        if ((instruction & 0xf800) == 0x6000) {
            return (instruction >> 0) & 0x7;
        } else if ((instruction & 0xf800) == 0x9000) {
            return (instruction >> 8) & 0x7;
        } else if ((instruction & 0xf800) == 0x5000) {
            return (instruction >> 0) & 0x7;
        } else if ((instruction & 0xfe00) == 0x5400) {
            return (instruction >> 0) & 0x7;
        } else if ((instruction & 0xf800) == 0x7000) {
            return (instruction >> 0) & 0x7;
        } else {
            printf("Unable to decode instruction 0x%04x\n", instruction);
            return -1;
        }
    }
}


uint32_t*
decode_rt(int reg, seL4_UserContext* c)
{
    switch (reg) {
    case  0:
        return &c->r0;
    case  1:
        return &c->r1;
    case  2:
        return &c->r2;
    case  3:
        return &c->r3;
    case  4:
        return &c->r4;
    case  5:
        return &c->r5;
    case  6:
        return &c->r6;
    case  7:
        return &c->r7;
    case  8:
        return &c->r8;
    case  9:
        return &c->r9;
    case 10:
        return &c->r10;
    case 11:
        return &c->r11;
    case 12:
        return &c->r12;
    case 13:
        return &c->sp;
    case 14:
        return &c->r14; /* lr */
    case 15:
        return &c->pc;
    default:
        assert(!"Invalid register");
        return NULL;
    }
};

int
fault_init(vm_t* vm, fault_t* fault)
{
    int err;
    fault->vm = vm;
    /* Reserve a slot for saving reply caps */
    err = vka_cspace_alloc_path(vm->vka, &fault->reply_cap);
    assert(!err);
    return err;
}

int
new_fault(fault_t* fault)
{
    uint32_t is_prefetch, ip, addr, fsr;
    seL4_CPtr tcb;
    int err;
    vm_t *vm;

    vm = fault->vm;
    assert(vm);
    tcb = vm_get_tcb(vm);

    /* First store message registers on the stack to free our message regs */
    is_prefetch = seL4_GetMR(SEL4_PFIPC_PREFETCH_FAULT);
    addr = seL4_GetMR(SEL4_PFIPC_FAULT_ADDR),
    fsr = seL4_GetMR(SEL4_PFIPC_FSR);
    ip = seL4_GetMR(SEL4_PFIPC_FAULT_IP);
    DFAULT("%s: New fault @ 0x%x from PC 0x%x\n", vm->name, addr, ip);
    /* Create the fault object */
    fault->is_prefetch = is_prefetch;
    fault->ip = ip;
    fault->addr = addr;
    fault->fsr = fsr;
    /* Gather additional information */
    assert(fault->reply_cap.capPtr);
    err = vka_cnode_saveCaller(&fault->reply_cap);
    assert(!err);
    err = seL4_TCB_ReadRegisters(tcb, false, 0,
                                 sizeof(fault->regs) / sizeof(fault->regs.pc),
                                 &fault->regs);
    assert(!err);
    /* Pull in data if we can */
    fault->data = 0xdeadbeef;
    if (fault_is_data(fault) && fault_is_write(fault)) {
        if (fault->fsr & HSR_SYNDROME_VALID) {
            int rt;
            if (HAS_ERRATA766422() && (fault->regs.cpsr & CPSR_THUMB)) {
                rt = errata766422_get_rt(fault, fault->fsr);
                if (rt < 0) {
                    assert(!"Could not decode RT");
                    return -1;
                }
            } else {
                rt = HSR_SYNDROME_RT(fault->fsr);
            }
            fault->data = *decode_rt(rt, &fault->regs);
        }
    }
    return 0;
}

int abandon_fault(fault_t* fault)
{
    /* Nothing to do here */
    DFAULT("%s: Release fault @ 0x%x from PC 0x%x\n",
           fault->vm->name, fault->addr, fault->ip);
    return 0;
}


int restart_fault(fault_t* fault)
{
    /* Send the reply */
    seL4_MessageInfo_t reply;
    reply = seL4_MessageInfo_new(0, 0, 0, 0);
    DFAULT("%s: Restart fault @ 0x%x from PC 0x%x\n",
           fault->vm->name, fault->addr, fault->ip);
    seL4_Send(fault->reply_cap.capPtr, reply);
    /* Clean up */
    return abandon_fault(fault);
}

int ignore_fault(fault_t* fault)
{
    seL4_UserContext * regs;
    seL4_CPtr tcb;
    int err;

    regs = &fault->regs;
    /* Advance the PC */
    regs->pc += fault_is_32bit_instruction(fault) ? 4 : 2;
    /* Write back CPU registers */
    tcb = vm_get_tcb(fault->vm);
    err = seL4_TCB_WriteRegisters(tcb, false, 0,
                                  sizeof(*regs) / sizeof(regs->pc), regs);
    assert(!err);
    if (err) {
        abandon_fault(fault);
        return err;
    }
    /* Reply to thread */
    return restart_fault(fault);
}

int advance_fault(fault_t* fault)
{
    seL4_UserContext * regs;
    regs = &fault->regs;
    /* If data was to be read, load it into the user context */
    if (fault_is_data(fault) && fault_is_read(fault)) {
        int rt;
        uint32_t* reg;
        if (!(fault->fsr & (1U << 24))) {
            print_fault(fault);
        }
        assert(fault->fsr & (1U << 24));
        rt = (fault->fsr >> 16) & 0xf;
        reg = decode_rt(rt, regs);
        *reg = fault_emulate(fault, *reg);
    }
    DFAULT("%s: Emulate fault @ 0x%x from PC 0x%x\n",
           fault->vm->name, fault->addr, fault->ip);
    return ignore_fault(fault);
}

uint32_t fault_emulate(fault_t* f, uint32_t o)
{
    uint32_t n, m;
    m = fault_get_data_mask(f);
    n = f->data;
    return (o & ~m) | (n & m);
}


void print_ctx_regs(seL4_UserContext * regs)
{
#define PREG(regs, r)    printf(#r ": 0x%x\n", regs->r)
    PREG(regs, r0);
    PREG(regs, r1);
    PREG(regs, r2);
    PREG(regs, r3);
    PREG(regs, r4);
    PREG(regs, r5);
    PREG(regs, r6);
    PREG(regs, r7);
    PREG(regs, r8);
    PREG(regs, r9);
    PREG(regs, r10);
    PREG(regs, r11);
    PREG(regs, r12);
    PREG(regs, pc);
    PREG(regs, r14);
    PREG(regs, sp);
    PREG(regs, cpsr);
#undef PREG
}

void print_fault(fault_t* fault)
{
    printf("--------\n");
    printf(COLOR_ERROR);
    printf("Pagefault from [%s]: %s %s "
           "@ PC: 0x"XFMT" IPA: 0x"XFMT", FSR: 0x"XFMT "\n",
           fault->vm->name,
           fault_is_read(fault) ? "read" : "write",
           fault_is_prefetch(fault) ? "prefetch fault" : "fault",
           fault->ip,
           fault->addr,
           fault->fsr);
    printf("Context:\n");
    print_ctx_regs(&fault->regs);
    printf(COLOR_NORMAL);
    printf("--------\n");
}

uint32_t fault_get_data_mask(fault_t* f)
{
    uint32_t mask = 0;
    uint32_t addr = f->addr;
    switch (fault_get_width(f)) {
    case WIDTH_BYTE:
        mask = 0x000000ff;
        assert(!(addr & 0x0));
        break;
    case WIDTH_HALFWORD:
        mask = 0x0000ffff;
        assert(!(addr & 0x1));
        break;
    case WIDTH_WORD:
        mask = 0xffffffff;
        assert(!(addr & 0x3));
        break;
    default:
        /* Should never get here... Keep the compiler happy */
        assert(0);
        return 0;
    }
    mask <<= (addr & 0x3) * 8;
    return mask;
}

enum fault_width fault_get_width(fault_t* f)
{
    switch ((f->fsr >> 22) & 0x3) {
    case 0:
        return WIDTH_BYTE;
    case 1:
        return WIDTH_HALFWORD;
    case 2:
        return WIDTH_WORD;
    default:
        assert(0);
        return 0;
    }
}


