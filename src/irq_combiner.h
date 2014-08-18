/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef IRQ_COMBINER_H
#define IRQ_COMBINER_H

#include "vm.h"
#include "irq_server.h"

struct combiner_irq {
    int group;
    int index;
    void* combiner_priv;
    void* priv;
};

typedef void (*combiner_irq_handler_fn)(struct combiner_irq* irq);

extern const struct device dev_irq_combiner;

/**
 * Register for an IRQ combiner IRQ
 * @param[in] group   The IRQ group (IRQ number - 32)
 * @param[in[ index   The irq index withing this group
 * @param[in] vb      A callback function to call when this IRQ fires
 * @param[in] priv    private data to pass to cb
 * @param[in] r       resources for allocation
 * @return            0 on succes
 */
int vmm_register_combiner_irq(irq_server_t irq_server, int group, int index,
                              combiner_irq_handler_fn cb, void* priv);

/**
 * Call to acknlowledge an IRQ with the combiner
 * @param[in] irq a description of the IRQ to ACK
 */
void combiner_irq_ack(struct combiner_irq* irq);


#endif /* IRQ_COMBINER_H */
