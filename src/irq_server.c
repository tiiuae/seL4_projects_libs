/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */


#include "irq_server.h"

#include <sel4utils/thread.h>
#include <vka/capops.h>
#include <stdlib.h>
#include <string.h>

#define IRQ_THREAD_PRIORITY  200
#define NIRQS_PER_NODE        20

struct irq_sys_node {
    struct irq irqs[NIRQS_PER_NODE];
    sel4utils_thread_t thread;
    vka_object_t aep;
    seL4_CPtr delivery_sep;
    seL4_Word label;
    /* Chain */
    struct irq_sys_node* next;
};

struct irq_sys {
    struct irq_sys_node* head;
    seL4_CPtr delivery_ep;
    seL4_Word label;
    int max_irqs;
    vspace_t* vspace;
    vka_t* vka;
    simple_t* simple;
};


void ack_irq(struct irq* irq)
{
    assert(irq);
    assert(irq->cap);
    seL4_IRQHandler_Ack(irq->cap);
}



static void
irq_sys_handle_irq(struct irq_sys_node* n, uint32_t badge)
{
    struct irq* irqs = n->irqs;
    while (badge) {
        int irq_idx;
        struct irq* irq;
        irq_idx = __builtin_ctz(badge);
        badge &= ~(1U << irq_idx);
        irq = &irqs[irq_idx];
        irq->cb(irq);
    }
}

void
irq_sys_handle_irq_msg(irq_sys_t irq_sys)
{
    uint32_t badge;
    uint32_t node_ptr;

    (void)irq_sys;
    badge = seL4_GetMR(0);
    node_ptr = seL4_GetMR(1);
    assert(node_ptr);

    irq_sys_handle_irq((struct irq_sys_node*)node_ptr, badge);
}

struct irq*
find_pirq(irq_sys_t irq_sys, int pirq) {
    struct irq_sys_node* n;
    int i;
    for (n = irq_sys->head; n != NULL; n = n->next) {
        for (i = 0; i < NIRQS_PER_NODE; i++) {
            if (n->irqs[i].pirq == pirq) {
                return &n->irqs[i];
            }
        }
    }
    return NULL;
}

struct irq*
find_virq(irq_sys_t irq_sys, int virq) {
    struct irq_sys_node* n;
    int i;
    for (n = irq_sys->head; n != NULL; n = n->next) {
        for (i = 0; i < NIRQS_PER_NODE; i++) {
            if (n->irqs[i].virq == virq) {
                return &n->irqs[i];
            }
        }
    }
    return NULL;
}

static void
_irq_thread_entry(struct irq_sys_node* n)
{
    seL4_CPtr sep;
    seL4_CPtr aep;
    uint32_t node_ptr;
    seL4_Word label;

    sep = n->delivery_sep;
    aep = n->aep.cptr;
    node_ptr = (uint32_t)n;
    label = n->label;

    while (1) {
        seL4_MessageInfo_t info;
        seL4_Word badge;

        info = seL4_Wait(aep, &badge);
        info = seL4_MessageInfo_new(label, 0, 0, 2);
        seL4_SendWithMRs(sep, info, &badge, &node_ptr, NULL, NULL);
    }
}

static struct irq_sys_node*
irq_node_new(vspace_t* vspace, vka_t* vka, simple_t* simple,
             seL4_Word label, seL4_CPtr sep) {
    struct irq_sys_node* n;
    seL4_CPtr cnode;
    int err;

    cnode = simple_get_cnode(simple);

    /* Allocate memory for the structure */
    n = (struct irq_sys_node*)malloc(sizeof(*n));
    memset(n, 0, sizeof(*n));
    n->delivery_sep = sep;
    n->label = label;
    /* Create an endpoint to listen on */
    err = vka_alloc_async_endpoint(vka, &n->aep);
    assert(!err);
    /* Create the IRQ thread */
    err = sel4utils_configure_thread(vka, vspace, seL4_CapNull,
                                     IRQ_THREAD_PRIORITY, cnode, seL4_NilData,
                                     &n->thread);
    assert(!err);
    err = sel4utils_start_thread(&n->thread, (void*)_irq_thread_entry, n, NULL, 1);
    assert(!err);
    return n;
}


static seL4_CPtr
irq_bind(int irq, seL4_CPtr aep_cap, int idx, vka_t* vka, simple_t* simple)
{
    seL4_CPtr irq_cap, baep_cap;
    cspacepath_t irq_path, aep_path, baep_path;
    seL4_CapData_t badge;
    int err;

    /* Create an IRQ cap */
    err = vka_cspace_alloc(vka, &irq_cap);
    if (err != 0) {
        printf("Failed to allocate cslot for irq\n");
        return seL4_CapNull;
    }
    vka_cspace_make_path(vka, irq_cap, &irq_path);
    err = simple_get_IRQ_control(simple, irq, irq_path);
    if (err != seL4_NoError) {
        printf("Failed to get cap to irq_number %u\n", irq);
        assert(0);
        vka_cspace_free(vka, irq_cap);
        return seL4_CapNull;
    }
    /* Badge the endpoint */
    err = vka_cspace_alloc(vka, &baep_cap);
    if (err != 0) {
        printf("Failed to allocate cslot for irq\n");
        vka_cspace_free(vka, irq_cap);
        return seL4_CapNull;
    }
    vka_cspace_make_path(vka, aep_cap, &aep_path);
    vka_cspace_make_path(vka, baep_cap, &baep_path);
    badge = seL4_CapData_Badge_new(1U << idx);
    err = vka_cnode_mint(&baep_path, &aep_path, seL4_AllRights, badge);
    assert(!err);
    if (err != seL4_NoError) {
        vka_cspace_free(vka, irq_cap);
        vka_cspace_free(vka, baep_cap);
        assert(0);
        return seL4_CapNull;
    }
    /* bind the IRQ cap to our badged endpoint */
    err = seL4_IRQHandler_SetEndpoint(irq_cap, baep_cap);
    if (err != seL4_NoError) {
        printf("seL4_IRQHandler_SetEndpoint failed with err %d\n", err);
        vka_cspace_free(vka, irq_cap);
        vka_cspace_free(vka, baep_cap);
        assert(0);
        return seL4_CapNull;
    }
    /* Finally ACK any pending IRQ */
    seL4_IRQHandler_Ack(irq_cap);
    return irq_cap;
}

static int
irq_sys_node_register_irq(struct irq_sys_node* n, struct irq irq,
                          vka_t* vka, simple_t* simple)
{
    struct irq* irqs;
    int i;
    irqs = n->irqs;
    for (i = 0; i < NIRQS_PER_NODE; i++) {
        if (irqs[i].cap == seL4_CapNull) {
            irq.cap = irq_bind(irq.pirq, n->aep.cptr, i, vka, simple);
            irqs[i] = irq;
            return 0;
        }
    }
    return -1;
}

int
vmm_register_irq(irq_sys_t irq_sys, int pirq, int virq,
                 irq_handler_fn cb, void* priv)
{
    struct irq_sys_node* n;
    struct irq irq;

    irq.pirq = pirq;
    irq.virq = virq;
    irq.cb = cb;
    irq.priv = priv;

    /* See if we can tack onto a node */
    for (n = irq_sys->head; n != NULL; n = n->next) {
        int err;
        err = irq_sys_node_register_irq(n, irq, irq_sys->vka, irq_sys->simple);
        if (!err) {
            return err;
        }
    }
    /* Try to create a new node */
    if (n == NULL && irq_sys->max_irqs < 0) {
        int err;
        /* Create the node */
        n = irq_node_new(irq_sys->vspace, irq_sys->vka, irq_sys->simple,
                         irq_sys->label, irq_sys->delivery_ep);
        assert(n);
        n->next = irq_sys->head;
        irq_sys->head = n;
        err = irq_sys_node_register_irq(n, irq, irq_sys->vka, irq_sys->simple);
        assert(!err);
        if (!err) {
            return err;
        }
    }
    /* Give up */
    return -1;
}



int
irq_sys_init(vspace_t* vspace, vka_t* vka, simple_t* simple,
             seL4_CPtr sync_ep, seL4_Word label,
             int nirqs, irq_sys_t *ret_irq_sys)
{
    struct irq_sys* irq_sys;

    irq_sys = (struct irq_sys*)malloc(sizeof(*irq_sys));
    if (irq_sys == NULL) {
        return -1;
    }

    irq_sys->delivery_ep = sync_ep;
    irq_sys->label = label;
    irq_sys->max_irqs = nirqs;
    irq_sys->head = NULL;
    irq_sys->vka = vka;
    irq_sys->simple = simple;
    irq_sys->vspace = vspace;

    if (nirqs > -1) {
        int i;
        int nodes;
        struct irq_sys_node** node;
        node = &irq_sys->head;
        nodes = (nirqs + NIRQS_PER_NODE - 1) / NIRQS_PER_NODE;
        for (i = 0; i < nodes; i++) {
            *node = irq_node_new(vspace, vka, simple, irq_sys->label,
                                 irq_sys->delivery_ep);
            node = &(*node)->next;
        }
    }

    *ret_irq_sys = irq_sys;
    return 0;
}



