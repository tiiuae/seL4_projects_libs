/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "irq_combiner.h"
#include "../../devices.h"
#include <stdlib.h>
#include <string.h>
#include <platsupport/irq_combiner.h>

//#define DEBUG_COMBINER

#ifdef DEBUG_MAPPINGS
#define DCOMBINER(...) printf("Combiner:" __VA_ARGS__)
#else
#define DCOMBINER(...) do{}while(0)
#endif


struct combiner_gmap {
    uint32_t enable_set;
    uint32_t enable_clr;
    uint32_t status;
    uint32_t masked_status;
};



struct irq_combiner_map {
    struct combiner_gmap g[8];
    uint32_t res[32];
    uint32_t pending;
};



struct irq_group_data {
    combiner_irq_handler_fn cb;
    void* priv;
};

struct combiner_data {
    irq_combiner_t pcombiner;
    struct irq_group_data* data[32];
};

/* For virtualisation */
struct virq_combiner {
    /* What the VM reads */
    struct irq_combiner_map* vregs;
};

struct combiner_data _combiner;

static inline struct virq_combiner* vcombiner_priv_get_vcombiner(void* priv) {
    return (struct virq_combiner*)priv;
}

void
vm_combiner_irq_handler(vm_t* vm, int irq)
{
    struct combiner_data* combiner = &_combiner;
    struct combiner_irq* cirq;
    uint32_t imsr;
    int i, g;

    /* Decode group and index */
    g = irq - 32;
    imsr = irq_combiner_group_pending(&combiner->pcombiner, g);
    i = __builtin_ctz(imsr);
    /* Disable the IRQ */
    irq_combiner_enable_irq(&combiner->pcombiner, COMBINER_IRQ(g, i));

    /* Allocate a combiner IRQ structure */
    cirq = (struct combiner_irq*)malloc(sizeof(*cirq));
    assert(cirq);
    if (!cirq) {
        printf("No memory to allocate combiner IRQ\n");
        return;
    }
    cirq->priv = combiner->data[g][i].priv;
    cirq->index = i;
    cirq->group = g;
    cirq->combiner_priv = combiner;

    /* Forward the IRQ */
    combiner->data[g][i].cb(cirq);
}

void
combiner_irq_ack(struct combiner_irq* cirq)
{
    struct combiner_data* combiner;
    int g, i;
    assert(cirq->combiner_priv);
    combiner = (struct combiner_data*)cirq->combiner_priv;
    g = cirq->group;
    i = cirq->index;
    /* Re-enable the IRQ */
    irq_combiner_enable_irq(&combiner->pcombiner, COMBINER_IRQ(g, i));
    free(cirq);
}


int
vmm_register_combiner_irq(int group, int idx, combiner_irq_handler_fn cb, void* priv)
{
    struct combiner_data* combiner = &_combiner;
    assert(0 <= group && group < 32);
    assert(0 <= idx && idx < 8);

    /* If no IRQ's for this group yet, setup the group */
    if (combiner->data[group] == NULL) {
        void *addr;

        addr = malloc(sizeof(struct irq_group_data) * 8);
        assert(addr);
        if (addr == NULL) {
            return -1;
        }

        memset(addr, 0, sizeof(struct irq_group_data) * 8);
        combiner->data[group] = (struct irq_group_data*)addr;

        DCOMBINER("Registered combiner IRQ (%d, %d)\n", group, idx);
    }

    /* Register the callback */
    combiner->data[group][idx].cb = cb;
    combiner->data[group][idx].priv = priv;

    /* Enable the irq */
    irq_combiner_enable_irq(&combiner->pcombiner, COMBINER_IRQ(group, idx));
    return 0;
}

static int
vcombiner_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    struct virq_combiner* vcombiner;
    int offset;
    int gidx;
    uint32_t mask;
    uint32_t *reg;

    assert(d->priv);
    vcombiner = vcombiner_priv_get_vcombiner(d->priv);
    mask = fault_get_data_mask(fault);
    offset = fault_get_address(fault) - d->pstart;
    reg = (uint32_t*)( (uintptr_t)vcombiner->vregs + offset );
    gidx = offset / sizeof(struct combiner_gmap);
    assert(offset >= 0 && offset < sizeof(*vcombiner->vregs));

    if (offset == 0x100) {
        DCOMBINER("Fault on group pending register\n");
    } else if (offset < 0x80) {
        uint32_t data;
        int group, index;
        (void)group;
        (void)index;
        switch (offset / 4 % 4) {
        case 0:
            data = fault_get_data(fault);
            data &= mask;
            data &= ~(*reg);
            while (data) {
                int i;
                i = CTZ(data);
                data &= ~(1U << i);
                group = gidx * 4 + i / 8;
                index = i % 8;
                DCOMBINER("enable IRQ %d.%d (%d)\n", group, index, group + 32);
            }
            return ignore_fault(fault);
        case 1:
            data = fault_get_data(fault);
            data &= mask;
            data &= *reg;
            while (data) {
                int i;
                i = CTZ(data);
                data &= ~(1U << i);
                group = gidx * 4 + i / 8;
                index = i % 8;
                DCOMBINER("disable IRQ %d.%d (%d)\n", group, index, group + 32);
            }
            return ignore_fault(fault);
        case 2:
        case 3:
            /* Read only registers */
        default:
            DCOMBINER("Error handling register access at offset 0x%x\n", offset);
        }
    } else {
        DCOMBINER("Unknown register access at offset 0%x\n", offset);
    }

    return advance_fault(fault);
}



const struct device dev_irq_combiner = {
    .devid = DEV_IRQ_COMBINER,
    .name = "irq.combiner",
    .pstart = IRQ_COMBINER_PADDR,
    .size = 0x1000,
    .handle_page_fault = vcombiner_fault,
    .priv = NULL
};

int
vm_install_vcombiner(vm_t* vm)
{
    struct device combiner;
    struct virq_combiner* vcombiner;
    void* addr;
    int err;

    err = irq_combiner_init(IRQ_COMBINER0, vm->io_ops, &_combiner.pcombiner);
    if (err) {
        return -1;
    }

    /* Distributor */
    combiner = dev_irq_combiner;
    vcombiner = malloc(sizeof(*vcombiner));
    if (!vcombiner) {
        assert(!"Unable to malloc memory for VGIC");
        return -1;
    }

    addr = map_emulated_device(vm, &combiner);
    assert(addr);
    if (addr == NULL) {
        return -1;
    }
    memset(addr, 0, 0x1000);
    vcombiner->vregs = (struct irq_combiner_map*)addr;
    combiner.priv = (void*)vcombiner;
    err = vm_add_device(vm, &combiner);
    if (err) {
        free(vcombiner);
        return -1;
    }
    return 0;
}
