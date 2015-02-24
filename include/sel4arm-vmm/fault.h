/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef SEL4ARM_VMM_FAULT_H
#define SEL4ARM_VMM_FAULT_H

#include <stdint.h>
#include <vka/cspacepath_t.h>

typedef struct vm vm_t;

enum fault_width {
    WIDTH_DOUBLEWORD,
    WIDTH_WORD,
    WIDTH_HALFWORD,
    WIDTH_BYTE
};

/**
 * Data structure representating a fault
 */
struct fault {
/// The VM associated with the fault
    vm_t *vm;
/// Reply capability to the faulting TCB
    cspacepath_t reply_cap;
/// VM registers at the time of the fault
    seL4_UserContext regs;

/// The IPA address of the fault
    uint32_t addr;
/// The IPA of the instruction which caused the fault
    uint32_t ip;
/// The data which was to be written, or the data to return to the VM
    uint32_t data;
/// Fault status register (IL and ISS fields of HSR cp15 register)
    uint32_t fsr;
/// 'true' if the fault was a prefetch fault rather than a data fault
    bool is_prefetch;
/// For multiple str/ldr and 32 bit access, the fault is handled in stages
    int stage;
/// If the instruction requires fetching, cache it here
    uint32_t instruction;
/// The width of the fault
    enum fault_width width;
};
typedef struct fault fault_t;


/**
 * Initialise a fault structure.
 * The structure will be bound to a VM and a reply cap slot
 * will be reserved.
 * @param[in] vm    The VM that the fault structure should be bound to
 * @param[in] fault A Fault structure to initialise
 * @return          0 on success
 */
int fault_init(vm_t* vm, fault_t* fault);

/**
 * Populate an initialised fault structure with fault data obtained from
 * a pending fault IPC message. The reply cap to the faulting TCB will
 * also be saved.
 * @param[in] fault  A handle to a fault structure
 * @return           0 on success;
 */
int new_fault(fault_t* fault);

/**
 * Abandon the fault.
 * Performs any necessary clean up of the fault structure once a fault
 * has been serviced. The VM will not be restarted by a call to this function.
 * @param[in] fault  A handle to a fault structure
 * @return           0 on success;
 */
int abandon_fault(fault_t* fault);

/**
 * Restart the fault
 * Return execution to the VM without modifying registers or advancing the
 * program counter. This is useful when the suitable response to the fault
 * is to map a frame.
 * @param[in] fault  A handle to a fault structure
 * @return           0 on success;
 */
int restart_fault(fault_t* fault);


/**
 * Ignore the fault.
 * Advances the PC without modifying registers. Useful when VM reads and
 * writes to invalid memory can be ignored.
 * @param[in] fault  A handle to a fault structure
 * @return           0 on success;
 */
int ignore_fault(fault_t* fault);


/**
 * Update register contents and return from a fault.
 * Updates user registers based on the data field of the fault struction
 * and replies to the faulting TCB to resume execution.
 * @param[in] fault  A handle to a fault structure
 * @param[in] data   The data word that the VM was attempting to access
 * @return           0 on success;
 */
int advance_fault(fault_t* fault);


/**
 * Emulates the faulting instruction of the VM on the provided data.
 * This function does not modify the state of the fault, it only returns
 * an updated representation of the given data based on the write operation
 * that the VM was attempting to perform.
 * @param[in] fault  A handle to a fault structure
 * @param[in] data   The data word that the VM was attempting to access
 * @return           The updated data based on the operation that the VM
 *                   was attempting to perform.
 */
uint32_t fault_emulate(fault_t* fault, uint32_t data);

/****************
 ***  Helpers ***
 ****************/

static inline int fault_handled(fault_t* fault)
{
    return fault->stage == 0;
}

static inline int fsr_is_WnR(uint32_t fsr)
{
    return (fsr & (1U << 6));
}

static inline int fault_is_read(fault_t* f)
{
    return !fsr_is_WnR(f->fsr);
}

static inline int fault_is_write(fault_t* f)
{
    return fsr_is_WnR(f->fsr);
}

static inline int fault_is_prefetch(fault_t* f)
{
    return f->is_prefetch;
}

static inline int fault_is_data(fault_t* f)
{
    return !f->is_prefetch;
}

static inline int fault_is_32bit_instruction(fault_t* f)
{
    return f->fsr & (1U << 25);
}

static inline int fault_is_16bit_instruction(fault_t* f)
{
    return !fault_is_32bit_instruction(f);
}

static inline enum fault_width fault_get_width(fault_t* f)
{
    return f->width;
}

uint32_t fault_get_data_mask(fault_t* f);

static inline uint32_t fault_get_addr_word(fault_t* f)
{
    return f->addr & ~(0x3U);
}

#include <stdio.h>

static inline void fault_print_data(fault_t* fault)
{
    uint32_t data;
    data = fault->data & fault_get_data_mask(fault);
    switch (fault_get_width(fault)) {
    case WIDTH_WORD:
        printf("0x%08x", data);
        break;
    case WIDTH_HALFWORD:
        printf("0x%04x", data);
        break;
    case WIDTH_BYTE:
        printf("0x%02x", data);
        break;
    default:
        printf("<Invalid width> 0x%x", data);
    }
}

/***************
 ***  Debug  ***
 ***************/

/**
 * Prints a fault to the console
 * @param[in] fault  A handle to a fault structure
 */
void print_fault(fault_t* fault);

/**
 * Prints contents of context registers to the console
 * @param[in] regs   A handle to the VM registers to print
 */
void print_ctx_regs(seL4_UserContext * regs);



#endif /* SEL4ARM_VMM_FAULT_H */
