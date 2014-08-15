/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef IRQ_SERVER_H
#define IRQ_SERVER_H


#include <sel4/sel4.h>
#include <vspace/vspace.h>
#include <simple/simple.h>
#include <vka/vka.h>

#define IRQ_MESSAGE_LABEL 0xCAFE
#define IRQ_FLAG_BIT 12

struct irq;
struct irq_sys;
typedef struct irq_sys* irq_sys_t;

/**
 * @return non-zero if the IRQ should be ACKed
 */
typedef void (*irq_handler_fn)(struct irq* irq);

struct irq {
    int pirq;
    int virq;
    irq_handler_fn cb;
    void* priv;
    /* Managed internally */
    seL4_CPtr cap;
};


/**
 * Enable an IRQ and register a callback function
 */
int vmm_register_irq(irq_sys_t irq_sys, int pirq, int virq,
                     irq_handler_fn cb, void* priv);

/**
 * Redirects control to the IRQ subsystem to process an arriving IRQ
 */
void irq_sys_handle_irq_msg(irq_sys_t irq_sys);

/**
 * Allows a client to acknowledge an IRQ
 */
void ack_irq(struct irq* irq);

struct irq* find_pirq(irq_sys_t irq_sys, int pirq);
struct irq* find_virq(irq_sys_t irq_sys, int virq);


/**
 * Initialises the IRQ subsystem.
 * Spawns enough threads to listen for nirqs irqs
 * Sends a sync IPC to sync_ep with message register 0 set to the IRQ number
 * When a message arrives, irq_sys_handle_irq should be called.
 * @param[in] vspace   The current vspace
 * @param[in] vka      Allocator for creating kernel objects
 * @param[in] simple   For retrieving key capabilities
 * @param[in] sync_ep  The synchronous endpoint to send IRQ's to
 * @param[in] label    a label to use when sending a sync IPC
 * @param[in] nirqs    The maximum number of irqs to support. -1 will set up
 *                     a dynamic system, however, the approriate resource
 *                     handles must remain available for the live of the server
 * @param[out] irq_sys An IRQ subsystem structure to initialise;
 * @return             0 on success
 */
int irq_sys_init(vspace_t* vspace, vka_t* vka, simple_t* simple,
                 seL4_CPtr sync_ep, seL4_Word label,
                 int nirqs, irq_sys_t* irq_sys);


#endif /* IRQ_SERVER_H */
