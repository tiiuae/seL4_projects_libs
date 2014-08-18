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

#if (defined CONFIG_LIB_SEL4_VKA && defined CONFIG_LIB_SEL4_VSPACE)

#include <sel4utils/thread.h>
#include <vka/capops.h>
#include <stdlib.h>
#include <string.h>


//#define DEBUG_IRQ_SERVER

#ifdef DEBUG_IRQ_SERVER
#define DIRQSERVER(...) printf("IRQ server: " __VA_ARGS__)
#else
#define DIRQSERVER(...) do{ }while(0)
#endif

#define NIRQS_PER_NODE        20

/*************************
 *** Generic functions ***
 *************************/

void
irq_data_ack_irq(struct irq_data* irq)
{
    assert(irq);
    assert(irq->cap);
    seL4_IRQHandler_Ack(irq->cap);
}

/***********************
 *** IRQ server node ***
 ***********************/

struct irq_server_node {
/// Information about the IRQ that is assigned to each badge bit
    struct irq_data irqs[NIRQS_PER_NODE];
/// The async endpoint that IRQs arrive on
    seL4_CPtr aep;
/// A mask for the badge. All set bits within the badge are treated as reserved.
    seL4_Word badge_mask;
};

/* Executes the registered callback for incoming IRQS */
static void
irq_server_node_handle_irq(struct irq_server_node *n, uint32_t badge)
{
    struct irq_data* irqs;
    irqs = n->irqs;
    /* Mask out reserved bits */
    badge = badge & n->badge_mask;
    /* For each bit, call the registered handler */
    while (badge) {
        int irq_idx;
        struct irq_data* irq;
        irq_idx = __builtin_ctz(badge);
        irq = &irqs[irq_idx];
        DIRQSERVER("Received IRQ %d, badge 0x%x, index %d\n", irq->irq, badge, irq_idx);
        irq->cb(irq);
        badge &= ~(1U << irq_idx);
    }
}

/* Binds and IRQ to an endpoint */
static seL4_CPtr
irq_bind(irq_t irq, seL4_CPtr aep_cap, int idx, vka_t* vka, seL4_CPtr irq_ctrl_cap)
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
    err = seL4_IRQControl_Get(seL4_CapIRQControl, irq, irq_path.root,
                              irq_path.capPtr, irq_path.capDepth);
    if (err != seL4_NoError) {
        printf("Failed to get cap to irq_number %u\n", irq);
        assert(0);
        vka_cspace_free(vka, irq_cap);
        return seL4_CapNull;
    }
    /* Badge the provided endpoint. The bit position of the badge tells us the array
     * index of the associated IRQ data. */
    err = vka_cspace_alloc(vka, &baep_cap);
    if (err != 0) {
        printf("Failed to allocate cslot for irq\n");
        vka_cspace_free(vka, irq_cap);
        return seL4_CapNull;
    }
    vka_cspace_make_path(vka, aep_cap, &aep_path);
    vka_cspace_make_path(vka, baep_cap, &baep_path);
    badge = seL4_CapData_Badge_new(BIT(idx));
    err = vka_cnode_mint(&baep_path, &aep_path, seL4_AllRights, badge);
    assert(!err);
    if (err != seL4_NoError) {
        vka_cspace_free(vka, irq_cap);
        vka_cspace_free(vka, baep_cap);
        return seL4_CapNull;
    }
    /* bind the IRQ cap to our badged endpoint */
    err = seL4_IRQHandler_SetEndpoint(irq_cap, baep_cap);
    if (err != seL4_NoError) {
        printf("seL4_IRQHandler_SetEndpoint failed with err %d\n", err);
        vka_cspace_free(vka, irq_cap);
        vka_cspace_free(vka, baep_cap);
        return seL4_CapNull;
    }
    /* Finally ACK any pending IRQ and enable the IRQ */
    seL4_IRQHandler_Ack(irq_cap);

    DIRQSERVER("Regestered IRQ %d with badge 0x%lx\n", irq, BIT(idx));
    return irq_cap;
}

/* Registers an IRQ callback and enabled the IRQ */
struct irq_data*
irq_server_node_register_irq(irq_server_node_t n, irq_t irq, irq_handler_fn cb,
                             void* token, vka_t* vka, seL4_CPtr cspace,
                             seL4_CPtr irq_ctrl_cap) {
    struct irq_data* irqs;
    int i;
    irqs = n->irqs;

    for (i = 0; i < NIRQS_PER_NODE; i++) {
        /* If a cap has not been registered and the bit in the mask is not set */
        if (irqs[i].cap == seL4_CapNull && (n->badge_mask & BIT(i))) {
            irqs[i].cap = irq_bind(irq, n->aep, i, vka, irq_ctrl_cap);
            if (irqs[i].cap == seL4_CapNull) {
                DIRQSERVER("Failed to bind IRQ\n");
                return NULL;
            }
            irqs[i].irq = irq;
            irqs[i].cb = cb;
            irqs[i].token = token;
            return &irqs[i];
        }
    }
    return NULL;
}

/* Creates a new IRQ server node which contains Thread data and registered IRQ data. */
struct irq_server_node*
irq_server_node_new(seL4_CPtr aep, seL4_Word badge_mask) {
    struct irq_server_node *n;
    n = (irq_server_node_t)malloc(sizeof(*n));
    if (n) {
        n->aep = aep;
        n->badge_mask = badge_mask;
        memset(n->irqs, 0, sizeof(n->irqs));
    }
    return n;
}

/*************************
 *** IRQ server thread ***
 *************************/

struct irq_server_thread {
/// IRQ data which this thread is responsible for
    struct irq_server_node *node;
/// A synchronous endpoint to deliver IRQ messages to.
    seL4_CPtr delivery_sep;
/// The label that should be assigned to outgoing synchronous messages.
    seL4_Word label;
/// Thread data
    sel4utils_thread_t thread;
/// Asynchronous endpoint object data
    vka_object_t aep;
/// Linked list chain
    struct irq_server_thread* next;
};

/* IRQ handler thread. Wait on an async EP for IRQs. When one arrives, send a
 * synchronous message to the registered endpoint. If no synchronous endpoint was
 * registered, call the appropriate handler function directly (must be thread safe) */
static void
_irq_thread_entry(struct irq_server_thread* st)
{
    seL4_CPtr sep;
    seL4_CPtr aep;
    uint32_t node_ptr;
    seL4_Word label;

    sep = st->delivery_sep;
    aep = st->node->aep;
    node_ptr = (uint32_t)st->node;
    label = st->label;
    DIRQSERVER("thread started. Waiting on endpoint %d\n", aep);

    while (1) {
        seL4_MessageInfo_t info;
        seL4_Word badge;
        info = seL4_Wait(aep, &badge);
        assert(badge != 0);
        if (sep != seL4_CapNull) {
            /* Synchronous endpoint registered. Send IPC */
            info = seL4_MessageInfo_new(label, 0, 0, 2);
            seL4_SendWithMRs(sep, info, &badge, &node_ptr, NULL, NULL);
        } else {
            /* No synchronous endpoint. Call the handler directly */
            irq_server_node_handle_irq(st->node, badge);
        }
    }
}

/* Creates a new thread for an IRQ server */
struct irq_server_thread*
irq_server_thread_new(vspace_t* vspace, vka_t* vka, seL4_CPtr cspace, seL4_Word priority,
                      seL4_CPtr irq_ctrl, seL4_Word label, seL4_CPtr sep) {
    struct irq_server_thread* st;
    int err;

    /* Allocate memory for the structure */
    st = (struct irq_server_thread*)malloc(sizeof(*st));
    if (st == NULL) {
        return NULL;
    }
    st->node = irq_server_node_new(0, (1U << NIRQS_PER_NODE) - 1);
    if (st->node == NULL) {
        free(st);
        return NULL;
    }

    /* Initialise structure */
    st->delivery_sep = sep;
    st->label = label;
    st->next = NULL;
    /* Create an endpoint to listen on */
    err = vka_alloc_async_endpoint(vka, &st->aep);
    assert(!err);
    st->node->aep = st->aep.cptr;
    /* Create the IRQ thread */
    err = sel4utils_configure_thread(vka, vspace, seL4_CapNull, priority,
                                     cspace, seL4_NilData, &st->thread);
    assert(!err);
    /* Start the thread */
    err = sel4utils_start_thread(&st->thread, (void*)_irq_thread_entry, st, NULL, 1);
    assert(!err);
    return st;
}


/******************
 *** IRQ server ***
 ******************/

struct irq_server {
    seL4_CPtr delivery_ep;
    seL4_Word label;
    int max_irqs;
    vspace_t* vspace;
    seL4_CPtr cspace;
    vka_t* vka;
    seL4_Word thread_priority;
    seL4_CPtr irq_ctrl_cap;
    struct irq_server_thread* server_threads;
};

/* Handle an incoming IPC from a server node */
void
irq_server_handle_irq_ipc(irq_server_t irq_server)
{
    uint32_t badge;
    uint32_t node_ptr;

    (void)irq_server;
    badge = seL4_GetMR(0);
    node_ptr = seL4_GetMR(1);
    assert(node_ptr);

    irq_server_node_handle_irq((struct irq_server_node*)node_ptr, badge);
}

/* Register for a function to be called when an IRQ arrives */
struct irq_data*
irq_server_register_irq(irq_server_t irq_server, irq_t irq,
                        irq_handler_fn cb, void* token) {
    struct irq_server_thread* st;
    struct irq_data* irq_data;

    /* Try to assign the IRQ to an existing node */
    for (st = irq_server->server_threads; st != NULL; st = st->next) {
        irq_data = irq_server_node_register_irq(st->node, irq, cb, token,
                                                irq_server->vka, irq_server->cspace,
                                                irq_server->irq_ctrl_cap);
        if (irq_data) {
            return irq_data;
        }
    }
    /* Try to create a new node */
    if (st == NULL && irq_server->max_irqs < 0) {
        /* Create the node */
        DIRQSERVER("Spawning new IRQ server thread\n");
        st = irq_server_thread_new(irq_server->vspace, irq_server->vka, irq_server->cspace,
                                   irq_server->thread_priority, irq_server->irq_ctrl_cap,
                                   irq_server->label, irq_server->delivery_ep);
        assert(st);
        st->next = irq_server->server_threads;
        irq_server->server_threads = st;
        irq_data = irq_server_node_register_irq(st->node, irq, cb, token,
                                                irq_server->vka, irq_server->cspace,
                                                irq_server->irq_ctrl_cap);
        if (irq_data) {
            return irq_data;
        }
    }
    /* Give up */
    DIRQSERVER("Failed to register for IRQ %d\n", irq);
    return NULL;
}

/* Create a new IRQ server */
int
irq_server_new(vspace_t* vspace, vka_t* vka, seL4_CPtr cspace, seL4_Word priority,
               seL4_CPtr irq_ctrl, seL4_CPtr sync_ep, seL4_Word label,
               int nirqs, irq_server_t *ret_irq_server)
{
    struct irq_server* irq_server;

    /* Structure allcoation and initialisation */
    irq_server = (struct irq_server*)malloc(sizeof(*irq_server));
    if (irq_server == NULL) {
        return -1;
    }
    irq_server->delivery_ep = sync_ep;
    irq_server->label = label;
    irq_server->max_irqs = nirqs;
    irq_server->vspace = vspace;
    irq_server->cspace = cspace;
    irq_server->vka = vka;
    irq_server->thread_priority = priority;
    irq_server->server_threads = NULL;

    /* If a fixed number of IRQs are requested, create and start the server threads */
    if (nirqs > -1) {
        struct irq_server_thread** server_thread;
        int n_nodes;
        int i;
        server_thread = &irq_server->server_threads;
        n_nodes = (nirqs + NIRQS_PER_NODE - 1) / NIRQS_PER_NODE;
        for (i = 0; i < n_nodes; i++) {
            *server_thread = irq_server_thread_new(vspace, vka, cspace, priority,
                                                   irq_ctrl, label, sync_ep);
            server_thread = &(*server_thread)->next;
        }
    }

    *ret_irq_server = irq_server;
    return 0;
}

#endif /* (defined CONFIG_LIB_SEL4_VKA && defined CONFIG_LIB_SEL4_VSPACE) */
