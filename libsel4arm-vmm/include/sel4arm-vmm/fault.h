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
#ifndef SEL4ARM_VMM_FAULT_H
#define SEL4ARM_VMM_FAULT_H

#include <stdint.h>
#include <vka/cspacepath_t.h>

typedef struct vm vm_t;
typedef struct fault fault_t;

enum fault_width {
    WIDTH_DOUBLEWORD,
    WIDTH_WORD,
    WIDTH_HALFWORD,
    WIDTH_BYTE
};

#define CPSR_THUMB                 BIT(5)
#define CPSR_IS_THUMB(x)           ((x) & CPSR_THUMB)

/**
 * Initialise a fault structure.
 * The structure will be bound to a VM and a reply cap slot
 * will be reserved.
 * @param[in] vm    The VM that the fault structure should be bound to
 * @return          An initialised fault structure handle or NULL on failure
 */
fault_t* fault_init(vm_t* vm);

/**
 * Populate an initialised fault structure with fault data obtained from
 * a pending VCPU fault message that is determined to be a WFI. The reply
 * cap to the faulting TCB will also be saved
 * @param[in] fault A handle to a fault structure
 * @return 0 on success;
 */
int new_wfi_fault(fault_t *fault);

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

/**
 * Determine if a fault has been handled. This is useful for multi-stage faults
 * @param[in] fault  A handle to the fault
 * @return           0 if the fault has not yet been handled, otherwise, further
 *                   action is required
 */
int fault_handled(fault_t* fault);

/**
 * Retrieve the data that the faulting thread is trying to write or the data
 * that will be returned to the thread in case of a read fault.
 * The fault must be a data fault.
 * @param[in] fault  A handle to the fault
 * @return           If it is a read fault, returns the data that will be returned
 *                   to the thread when the fault is advanced. Otherwise, returns
 *                   the data that the thread was attempting to write.
 */
uint32_t fault_get_data(fault_t* fault);

/**
 * Set the data that will be returned to the thread when the fault is advanced.
 * The fault must be a data fault.
 * @param[in] fault  A handle to the fault
 * @param[in] data   The data to return to the thread.
 */
void fault_set_data(fault_t* fault, uint32_t data);

/**
 * Retrieve a mask for the data within the aligned word that the fault is
 * attempting to access
 * @param[in] fault  A handle to the fault
 * @return           A mask for the data within the aligned word that the fault is
 *                   attempting to access
 */
uint32_t fault_get_data_mask(fault_t* fault);

/**
 * Get the faulting address
 * @param[in] fault  A handle to the fault
 * @return           The address that the faulting thread was attempting to access
 */
uint32_t fault_get_address(fault_t* fault);

/**
 * Get the access width of the fault
 * The fault must be a data fault.
 * @param[in] fault  A handle to the fault
 * @return           The access width of the fault
 */
enum fault_width fault_get_width(fault_t* f);

/**
 * Get the context of a fault
 * @param[in] fault  A handle to the fault
 * @return           A handle to the fault context
 */
seL4_UserContext *fault_get_ctx(fault_t *fault);

/**
 * Get the fault status register of a fault
 * @param[in] fault  A handle to the fault
 * @return           the ARM HSR register associated with this fault. The EC
 *                   field will be masked out.
 */
uint32_t fault_get_fsr(fault_t *fault);

/**
 * Determine if a fault is a data or prefetch fault
 * @param[in] fault  A handle to the fault
 * @return           0 if the fault is a data fault, otherwise, it is a prefetch
 *                   fault
 */
int fault_is_prefetch(fault_t* fault);

/**
 * Determine if we should wait for an interrupt before
 * resuming from the fault
 * @param[in] fault A handle to the fault
 */
int fault_is_wfi(fault_t *fault);

/**
 * Determine if a fault was caused by a 32 bit instruction
 * @param[in] fault  A handle to the fault
 * @return           0 if it is a 16 bit instruction, otherwise, it is 32bit
 */
int fault_is_32bit_instruction(fault_t* f);

/****************
 ***  Helpers ***
 ****************/

static inline int fault_is_16bit_instruction(fault_t* f)
{
    return !fault_is_32bit_instruction(f);
}

static inline int fault_is_data(fault_t* f)
{
    return !fault_is_prefetch(f);
}

static inline int fault_is_write(fault_t* f)
{
    return (fault_get_fsr(f) & (1U << 6));
}

static inline int fault_is_read(fault_t* f)
{
    return !fault_is_write(f);
}

static inline uint32_t fault_get_addr_word(fault_t* f)
{
    return fault_get_address(f) & ~(0x3U);
}

seL4_Word *decode_rt(int reg, seL4_UserContext *c);
int decode_vcpu_reg(int rt, fault_t *f);

#include <stdio.h>

static inline void fault_print_data(fault_t* fault)
{
    uint32_t data;
    data = fault_get_data(fault) & fault_get_data_mask(fault);
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
