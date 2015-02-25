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
#define DERRATA(...) do{}while(0)
#endif

#ifdef PLAT_EXYNOS5250
#define HAS_ERRATA766422() 1
#else
#define HAS_ERRATA766422() 0
#endif

#define COLOR_ERROR "\033[1;31m"
#define COLOR_NORMAL "\033[0m"

#define CPSR_THUMB                 BIT(5)
#define CPSR_IS_THUMB(x)           ((x) & CPSR_THUMB)
#define HSR_INST32                 BIT(25)
#define HSR_IS_INST32(x)           ((x) & HSR_INST32)
#define HSR_SYNDROME_VALID         BIT(24)
#define HSR_IS_SYNDROME_VALID(hsr) ((hsr) & HSR_SYNDROME_VALID)
#define HSR_SYNDROME_RT(x)         (((x) >> 16) & 0xf)
#define HSR_SYNDROME_WIDTH(x)      (((x) >> 22) & 0x3)

static inline int
thumb_is_32bit_instruction(uint32_t instruction)
{
    switch ((instruction >> 11) & 0x1f) {
    case 0b11101:
    case 0b11110:
    case 0b11111:
        return 1;
    default:
        return 0;
    }
}

static int
fetch_fault_instruction(fault_t* f)
{
    uint32_t inst = 0;
    /* Fetch the instruction */
    if (vm_copyin(f->vm, &inst, f->ip, 4) < 0) {
        return -1;
    }
    /* Fixup the instruction */
    if (CPSR_IS_THUMB(f->regs.cpsr)) {
        if (thumb_is_32bit_instruction(inst)) {
            f->fsr |= HSR_INST32;
        }
        if (HSR_IS_INST32(f->fsr)) {
            /* Swap half words for a 32 bit instruction */
            inst = ((inst & 0xffff)) << 16 | ((inst >> 16) & 0xffff);
        } else {
            /* Mask instruction for 16 bit instruction */
            inst &= 0xffff;
        }
    } else {
        /* All ARM instructions are 32 bit so force the HSR flag to be set */
        f->fsr |= HSR_INST32;
    }
    f->instruction = inst;
    return 0;
}

static int
errata766422_get_rt(fault_t* f, uint32_t hsr)
{
    uint32_t instruction;
    int err;
    err = fetch_fault_instruction(f);
    instruction = f->instruction;
    if (err) {
        return err;
    }
    if (HSR_IS_INST32(hsr)) {
        DERRATA("Errata766422 @ 0x%08x (0x%08x)\n", f->regs.pc, instruction);
        if ((instruction & 0xff700000) == 0xf8400000) {
            return (instruction >> 12) & 0xf;
        } else if ((instruction & 0xfff00000) == 0xf8800000) {
            return (instruction >> 12) & 0xf;
        } else if ((instruction & 0xfff00000) == 0xf0000000) {
            return (instruction >> 12) & 0xf;
        } else if ((instruction & 0x0e500000) == 0x06400000) {
            return (instruction >> 12) & 0xf;
        } else if ((instruction & 0xfff00000) == 0xf8000000) {
            return (instruction >> 12) & 0xf;
        } else {
            printf("Unable to decode instruction 0x%08x\n", instruction);
            return -1;
        }
    } else {
        DERRATA("Errata766422 @ 0x%08x (0x%04x)\n", f->regs.pc, instruction);
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
        } else if ((instruction & 0xf800) == 0x8000) {
            return (instruction >> 0) & 0x7;
        } else {
            printf("Unable to decode instruction 0x%04x\n", instruction);
            return -1;
        }
    }
}

static int
decode_instruction(fault_t* f, int in_init)
{
    uint32_t instruction;
    if (in_init && fetch_fault_instruction(f)) {
        return -1;
    }
    instruction = f->instruction;
    if (CPSR_IS_THUMB(f->regs.cpsr)) {
        if (HSR_IS_INST32(f->fsr)) {
            /* 32 BIT THUMB instructions */
            if ((instruction & 0xff700000) == 0xf8400000) {
                return (instruction >> 12) & 0xf;
            } else if ((instruction & 0xfff00000) == 0xf8800000) {
                return (instruction >> 12) & 0xf;
            } else if ((instruction & 0xfff00000) == 0xf0000000) {
                return (instruction >> 12) & 0xf;
            } else if ((instruction & 0x0e500000) == 0x06400000) {
                return (instruction >> 12) & 0xf;
            } else if ((instruction & 0xfff00000) == 0xf8000000) {
                return (instruction >> 12) & 0xf;
            } else if ((instruction & 0xfe400000) == 0xe8400000) { /* LDRD/STRD */
                int rt;
                if (in_init) {
                    f->stage = 2;
                    f->width = WIDTH_DOUBLEWORD;
                }
                f->addr = f->base_addr + ((2 - f->stage) * sizeof(uint32_t));
                rt = ((instruction >> 12) & 0xf) + (2 - f->stage);
                return rt;
            } else {
                printf("Unable to decode THUMB32 instruction 0x%08x\n", instruction);
                print_fault(f);
                return -1;
            }
        } else {
            /* 16 bit THUMB instructions */
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
            } else if ((instruction & 0xf800) == 0x8000) {
                return (instruction >> 0) & 0x7;
            } else {
                printf("Unable to decode THUMB16 instruction 0x%04x\n", instruction);
                print_fault(f);
                return -1;
            }
        }
    } else {
        printf("32 bit ARM instructions not decoded\n");
        print_fault(f);
        return -1;
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

static int
decode_fault(fault_t* f)
{
    /* Defaults */
    f->addr = f->base_addr;
    f->stage = 1;
    /* If it is a data fault, prepare the fault details */
    if (fault_is_data(f)) {
        /* Read or write, we need to find the width */
        if (HSR_IS_SYNDROME_VALID(f->fsr)) {
            switch (HSR_SYNDROME_WIDTH(f->fsr)) {
            case 0:
                f->width = WIDTH_BYTE;
                break;
            case 1:
                f->width = WIDTH_HALFWORD;
                break;
            case 2:
                f->width = WIDTH_WORD;
                break;
            default:
                print_fault(f);
                assert(0);
                return 0;
            }
        }
        if (fault_is_write(f)) {
            /* If it is a write fault, we fill the data field if the fault */
            int rt;
            if (HSR_IS_SYNDROME_VALID(f->fsr)) {
                rt = HSR_SYNDROME_RT(f->fsr);
                /* Stores in thumb mode may trigger errata */
                if (HAS_ERRATA766422() && CPSR_IS_THUMB(f->regs.cpsr)) {
                    rt = errata766422_get_rt(f, f->fsr);
                }
            } else {
                rt = decode_instruction(f, 1);
            }
            assert(rt >= 0);
            f->data = *decode_rt(rt, &f->regs);
        } else {
            /* If it is a read fault, just ensure we know the width for now */
            if (!HSR_IS_SYNDROME_VALID(f->fsr)) {
                int rt;
                rt = decode_instruction(f, 1);
                assert(rt >= 0);
            }
        }
    }
    return 0;
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
    fault->base_addr = addr;
    fault->fsr = fsr;
    fault->instruction = 0;
    fault->data = 0xdeadbeef;
    /* Gather additional information */
    assert(fault->reply_cap.capPtr);
    err = vka_cnode_saveCaller(&fault->reply_cap);
    assert(!err);

    err = seL4_TCB_ReadRegisters(tcb, false, 0,
                                 sizeof(fault->regs) / sizeof(fault->regs.pc),
                                 &fault->regs);
    assert(!err);

    err = decode_fault(fault);
    assert(!err);
    return err;
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
    fault->stage = 0;
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
        if (!HSR_IS_SYNDROME_VALID(fault->fsr)) {
            rt = decode_instruction(fault, 0);
        } else {
            rt = HSR_SYNDROME_RT(fault->fsr);
        }
        reg = decode_rt(rt, regs);
        *reg = fault_emulate(fault, *reg);
    }
    DFAULT("%s: Emulate fault @ 0x%x from PC 0x%x\n",
           fault->vm->name, fault->addr, fault->ip);
    /* If this is the final stage of the fault, return to user */
    fault->stage--;
    if (fault->stage) {
        /* For multi stage instructions, we know that the HSR is invalide.
         * Decode the instruction again with the new stage set to update
         * address and data.
         */
        int rt;
        rt = decode_instruction(fault, 0);
        assert(rt >= 0);
        fault->data = *decode_rt(rt, &fault->regs);
        return 0;
    } else {
        return ignore_fault(fault);
    }
}

uint32_t fault_emulate(fault_t* f, uint32_t o)
{
    uint32_t n, m, s;
    s = (f->addr & 0x3) * 8;
    m = fault_get_data_mask(f);
    n = f->data;
    if (fault_is_read(f)) {
        /* Read data must be shifted to lsb */
        return (o & ~(m >> s)) | ((n & m) >> s);
    } else {
        /* Data to write must be shifted left to compensate for alignment */
        return (o & ~m) | ((n << s) & m);
    }
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
    case WIDTH_DOUBLEWORD:
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


