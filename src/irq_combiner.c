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
#include "devices.h"
#include <stdlib.h>
#include <string.h>
#include "irq_server.h"

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
    volatile struct irq_combiner_map* regs;
    struct irq_group_data* data[32];
    irq_server_t irq_server;
};

/* For virtualisation */
struct virq_combiner {
    /* What the VM reads */
    struct irq_combiner_map* vregs;
};

struct combiner_data _combiner = { .regs = NULL };


static void
combiner_irq_handler(struct irq_data* irq)
{
    struct combiner_data* combiner;
    struct combiner_irq* cirq;
    uint32_t imsr;
    int i, g;

    assert(irq->token);
    combiner = (struct combiner_data*)irq->token;

    /* Decode group and index */
    g = irq->irq - 32;
    imsr = combiner->regs->g[g / 8].masked_status;
    i = __builtin_ctz((imsr >> (g & 0x3) * 8) & 0xff);
    /* Disable the IRQ */
    combiner->regs->g[g / 8].enable_clr = (g & 0x3) * 8 + i;
    irq_data_ack_irq(irq);

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
    combiner->regs->g[g / 8].enable_set = (g & 0x3) * 8 + i;
    free(cirq);
}


int
vmm_register_combiner_irq(irq_server_t irq_server, int group, int idx,
                          combiner_irq_handler_fn cb, void* priv)
{
    struct combiner_data* combiner = &_combiner;
    assert(0 <= group && group < 32);
    assert(0 <= idx && idx < 8);

    /* If no IRQ's for this group yet, setup the group */
    if (combiner->data[group] == NULL) {
        struct irq_data* irq_data;
        void *addr;
        irq_t pirq;

        addr = malloc(sizeof(struct irq_group_data) * 8);
        assert(addr);
        if (addr == NULL) {
            return -1;
        }

        memset(addr, 0, sizeof(struct irq_group_data) * 8);
        combiner->data[group] = (struct irq_group_data*)addr;

        pirq = group + 32;
        DCOMBINER("Registered IRQ %d\n", pirq);
        irq_data = irq_server_register_irq(irq_server, pirq, &combiner_irq_handler, combiner);
        assert(irq_data);
        if (!irq_data) {
            return -1;
        }
    }

    /* Register the callback */
    combiner->data[group][idx].cb = cb;
    combiner->data[group][idx].priv = priv;

    /* Enable the irq */
    combiner->regs->g[group / 8].enable_set = (group & 0x3) * 8 + idx;
    return 0;
}

static inline struct virq_combiner* vcombiner_priv_get_vcombiner(void* priv) {
    return (struct virq_combiner*)priv;
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
    offset = fault->addr - d->pstart;
    reg = (uint32_t*)( (uintptr_t)vcombiner->vregs + offset );
    gidx = offset / sizeof(struct combiner_gmap);
    assert(offset >= 0 && offset < sizeof(*vcombiner->vregs));

    if (offset == 0x100) {
        DCOMBINER("Fault on group pending register\n");
    } else if (offset < 0x80) {
        int group, index;
        (void)group;
        (void)index;
        switch (offset / 4 % 4) {
        case 0:
            fault->data &= mask;
            fault->data &= ~(*reg);
            while (fault->data) {
                int i;
                i = __builtin_ctz(fault->data);
                fault->data &= ~(1U << i);
                group = gidx * 4 + i / 8;
                index = i % 8;
                DCOMBINER("enable IRQ %d.%d (%d)\n", group, index, group + 32);
            }
            return ignore_fault(fault);
        case 1:
            fault->data &= mask;
            fault->data &= *reg;
            while (fault->data) {
                int i;
                i = __builtin_ctz(fault->data);
                fault->data &= ~(1U << i);
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
    .devid = DEV_MCT_TIMER,
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
    if (_combiner.regs == NULL) {
        _combiner.regs = map_device(vm->vmm_vspace, vm->vka, vm->simple,
                                    IRQ_COMBINER_PADDR, 0, seL4_AllRights);
        assert(_combiner.regs);
        if (!_combiner.regs) {
            return -1;
        }
        _combiner.irq_server = vm->irq_server;
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
