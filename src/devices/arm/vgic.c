/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "vgic.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <vka/vka.h>
#include <vka/capops.h>

#include "../../devices.h"
#include "../../vm.h"
#include "../../irq_server.h"

//#define DEBUG_IRQ
//#define DEBUG_DIST

#ifdef DEBUG_IRQ
#define DIRQ(...) printf(__VA_ARGS__)
#else
#define DIRQ(...) do{}while(0)
#endif

#ifdef DEBUG_DIST
#define DDIST(...) printf(__VA_ARGS__)
#else
#define DDIST(...) do{}while(0)
#endif



#ifdef PLAT_EXYNOS5
#define GIC_PADDR 0x10480000
#else
#error Unknown SoC
#endif

#define GIC_DIST_PADDR       (GIC_PADDR + 0x1000)
#define GIC_CPU_PADDR        (GIC_PADDR + 0x2000)
#define GIC_VCPU_CNTR_PADDR  (GIC_PADDR + 0x4000)
#define GIC_VCPU_PADDR       (GIC_PADDR + 0x6000)

#define IRQ_IDX(irq) ((irq) / 32)
#define IRQ_BIT(irq) (1U << ((irq) % 32))

enum gic_dist_action {
    ACTION_READONLY,
    ACTION_PASSTHROUGH,
    ACTION_ENABLE,
    ACTION_ENABLE_SET,
    ACTION_ENABLE_CLR,
    ACTION_PENDING_SET,
    ACTION_PENDING_CLR,
    ACTION_SGI,
    ACTION_UNKNOWN
};



/* Memory map for GIC distributer */
struct gic_dist_map {
    uint32_t enable;                /* 0x000 */
    uint32_t ic_type;               /* 0x004 */
    uint32_t dist_ident;            /* 0x008 */
    uint32_t res1[29];              /* [0x00C, 0x080) */

    uint32_t security[32];          /* [0x080, 0x100) */

    uint32_t enable_set[32];        /* [0x100, 0x180) */
    uint32_t enable_clr[32];        /* [0x180, 0x200) */
    uint32_t pending_set[32];       /* [0x200, 0x280) */
    uint32_t pending_clr[32];       /* [0x280, 0x300) */
    uint32_t active[32];            /* [0x300, 0x380) */
    uint32_t res2[32];              /* [0x380, 0x400) */

    uint32_t priority[255];         /* [0x400, 0x7FC) */
    uint32_t res3;                  /* 0x7FC */

    uint32_t targets[255];            /* [0x800, 0xBFC) */
    uint32_t res4;                  /* 0xBFC */

    uint32_t config[64];             /* [0xC00, 0xD00) */

    uint32_t spi[32];               /* [0xD00, 0xD80) */
    uint32_t res5[20];              /* [0xD80, 0xDD0) */
    uint32_t res6;                  /* 0xDD0 */
    uint32_t legacy_int;            /* 0xDD4 */
    uint32_t res7[2];               /* [0xDD8, 0xDE0) */
    uint32_t match_d;               /* 0xDE0 */
    uint32_t enable_d;              /* 0xDE4 */
    uint32_t res8[70];               /* [0xDE8, 0xF00) */

    uint32_t sgi_control;           /* 0xF00 */
    uint32_t res9[3];               /* [0xF04, 0xF10) */
    uint32_t sgi_pending_clr[4];    /* [0xF10, 0xF20) */
    uint32_t res10[40];             /* [0xF20, 0xFC0) */

    uint32_t periph_id[12];         /* [0xFC0, 0xFF0) */
    uint32_t component_id[4];       /* [0xFF0, 0xFFF] */
};

struct lr_of {
    struct irq* irq;
    struct lr_of* next;
};

struct vgic {
    struct irq* irq[63];
    struct lr_of* lr_overflow;
    struct gic_dist_map *dist;
};



static void vgic_dist_irq_handler(struct irq* irq);



static inline struct vgic* vgic_priv_get_priv(void* priv) {
    assert(priv);
    return (struct vgic*)priv;
}

static inline struct gic_dist_map* vgic_priv_get_dist(void* priv) {
    assert(priv);
    return vgic_priv_get_priv(priv)->dist;
}

static inline struct irq** vgic_priv_get_lr(void* priv) {
    assert(priv);
    return vgic_priv_get_priv(priv)->irq;
}



static inline void set_pending(struct gic_dist_map* gic_dist, int irq, int v)
{
    if (v) {
        gic_dist->pending_set[IRQ_IDX(irq)] |= IRQ_BIT(irq);
        gic_dist->pending_clr[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->pending_set[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
        gic_dist->pending_clr[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline int is_pending(struct gic_dist_map* gic_dist, int irq)
{
    return !!(gic_dist->pending_set[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static inline void set_enable(struct gic_dist_map* gic_dist, int irq, int v)
{
    if (v) {
        gic_dist->enable_set[IRQ_IDX(irq)] |= IRQ_BIT(irq);
        gic_dist->enable_clr[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->enable_set[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
        gic_dist->enable_clr[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline int is_enabled(struct gic_dist_map* gic_dist, int irq)
{
    return !!(gic_dist->enable_set[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static inline void set_active(struct gic_dist_map* gic_dist, int irq, int v)
{
    if (v) {
        gic_dist->active[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->active[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline int is_active(struct gic_dist_map* gic_dist, int irq)
{
    return !!(gic_dist->active[IRQ_IDX(irq)] & IRQ_BIT(irq));
}


static int
vgic_vcpu_inject_irq(struct device* d, vm_t *vm, struct irq *irq)
{
    struct vgic* vgic;
    int err;
    int i;
    assert(d);
    assert(d->priv);
    vgic = (struct vgic*)d->priv;

    seL4_CPtr vcpu;
    vcpu = vm->vcpu.cptr;
    for (i = 0; i < 64; i++) {
        if (vgic->irq[i] == NULL) {
            break;
        }
    }
    err = seL4_ARM_VCPU_InjectIRQ(vcpu, irq->virq, 0, 0, i);
    assert((i < 4) || err);
    if (!err) {
        /* Shadow */
        vgic->irq[i] = irq;
        return err;
    } else {
        /* Add to overflow list */
        struct vgic* vgic = vgic_priv_get_priv(d->priv);
        struct lr_of** lrof_ptr;
        struct lr_of* lrof;
        lrof_ptr = &vgic->lr_overflow;
        while (*lrof_ptr != NULL) {
            lrof_ptr = &(*lrof_ptr)->next;
        }
        lrof = (struct lr_of*)malloc(sizeof(*lrof));
        assert(lrof);
        if (lrof == NULL) {
            return -1;
        }
        lrof->irq = irq;
        lrof->next = NULL;
        *lrof_ptr = lrof;
        return 0;
    }
}


int handle_vgic_maintenance(vm_t* vm, int idx)
{
    struct device* d;
    struct gic_dist_map* gic_dist;
    struct irq** lr;
    d = vm_find_device_by_id(vm, DEV_VGIC_DIST);
    assert(d);
    gic_dist = vgic_priv_get_dist(d->priv);
    lr = vgic_priv_get_lr(d->priv);

    if (lr[idx]) {
        struct lr_of** lrof_ptr;
        set_pending(gic_dist, lr[idx]->virq, false);
        ack_irq(lr[idx]);
        lr[idx] = NULL;
        lrof_ptr = &vgic_priv_get_priv(d->priv)->lr_overflow;
        if (*lrof_ptr) {
            struct lr_of* lrof;
            int err;
            lrof = *lrof_ptr;
            *lrof_ptr = lrof->next;
            err = vgic_vcpu_inject_irq(d, vm, lrof->irq);
            assert(!err);
            free(lrof);
        }
        return 0;
    } else {
        assert(!"Invalid index");
        return 1;
    }
}


static enum gic_dist_action gic_dist_get_action(int offset)
{
    /* Handle the fault
     * The only fields we care about are enable_set/clr
     * We have 2 options for other registers:
     *  a) ignore writes and hope the VM acts appropriately
     *  b) allow write access so the VM thinks there is no problem,
     *     but do not honour them
     */
    if (0x000 <= offset && offset < 0x004) {     /* enable          */
        return ACTION_ENABLE;
    } else if (0x080 <= offset && offset < 0x100) { /* Security        */
        return ACTION_PASSTHROUGH;
    } else if (0x100 <= offset && offset < 0x180) { /* enable_set      */
        return ACTION_ENABLE_SET;
    } else if (0x180 <= offset && offset < 0x200) { /* enable_clr      */
        return ACTION_ENABLE_CLR;
    } else if (0x200 <= offset && offset < 0x280) { /* pending_set     */
        return ACTION_PENDING_SET;
    } else if (0x280 <= offset && offset < 0x300) { /* pending_clr     */
        return ACTION_PENDING_CLR;
    } else if (0x300 <= offset && offset < 0x380) { /* active          */
        return ACTION_READONLY;
    } else if (0x400 <= offset && offset < 0x7FC) { /* priority        */
        return ACTION_READONLY;
    } else if (0x800 <= offset && offset < 0x8FC) { /* targets         */
        return ACTION_READONLY;
    } else if (0xC00 <= offset && offset < 0xD00) { /* config          */
        return ACTION_READONLY;
    } else if (0xD00 <= offset && offset < 0xD80) { /* spi config      */
        return ACTION_READONLY;
    } else if (0xDD4 <= offset && offset < 0xDD8) { /* legacy_int      */
        return ACTION_READONLY;
    } else if (0xDE0 <= offset && offset < 0xDE4) { /* match_d         */
        return ACTION_READONLY;
    } else if (0xDE4 <= offset && offset < 0xDE8) { /* enable_d        */
        return ACTION_READONLY;
    } else if (0xF00 <= offset && offset < 0xF04) { /* sgi_control     */
        return ACTION_PASSTHROUGH;
    } else if (0xF10 <= offset && offset < 0xF10) { /* sgi_pending_clr */
        return ACTION_SGI;
    } else {
        return ACTION_READONLY;
    }
    return ACTION_UNKNOWN;
}

static int
vgic_dist_enable(struct device* d, vm_t* vm)
{
    struct gic_dist_map* gic_dist = vgic_priv_get_dist(d->priv);
    DDIST("enabling gic distributer\n");
    gic_dist->enable = 1;
    return 0;
}

static int
vgic_dist_disable(struct device* d, vm_t* vm)
{
    struct gic_dist_map* gic_dist = vgic_priv_get_dist(d->priv);
    DDIST("disabling gic distributer\n");
    gic_dist->enable = 0;
    return 0;
}

static int
vgic_dist_enable_irq(struct device* d, vm_t* vm, int irq)
{
    struct gic_dist_map* gic_dist = vgic_priv_get_dist(d->priv);
    struct irq* irq_data;
    DDIST("enabling irq %d\n", irq);
    set_enable(gic_dist, irq, true);
    irq_data = find_virq(vm->irq_sys, irq);
    /* Register the IRQ if it does not exist */
    if (!irq_data) {
        int err;
        err = vmm_register_irq(vm->irq_sys, irq, irq, &vgic_dist_irq_handler, vm);
        assert(!err);
        if (err) {
            return err;
        }
    }
    /* If it is already pending, trigger the IRQ */
    if (is_pending(gic_dist, irq)) {
        int err;
        err = vgic_vcpu_inject_irq(d, vm, irq_data);
        assert(!err);
        return err;
    }
    return 0;
}

static int
vgic_dist_disable_irq(struct device* d, vm_t* vm, int irq)
{
    struct gic_dist_map* gic_dist = vgic_priv_get_dist(d->priv);
    if (irq >= 16) {
        DDIST("disabling irq %d\n", irq);
        set_enable(gic_dist, irq, false);
    }
    return 0;
}

static int
vgic_dist_set_pending_irq(struct device* d, vm_t* vm, int irq)
{
    struct gic_dist_map* gic_dist = vgic_priv_get_dist(d->priv);
    struct irq* irq_data;
    DDIST("set pending irq %d\n", irq);
    set_pending(gic_dist, irq, true);
    irq_data = find_virq(vm->irq_sys, irq);
    /* If it is enables, inject the IRQ */
    if (irq_data && gic_dist->enable && is_enabled(gic_dist, irq)) {
        int err;
        err = vgic_vcpu_inject_irq(d, vm, irq_data);
        assert(!err);
        return err;
    }
    return 0;
}

static int
vgic_dist_clr_pending_irq(struct device* d, vm_t* vm, int irq)
{
    struct gic_dist_map* gic_dist = vgic_priv_get_dist(d->priv);
    DDIST("clr pending irq %d\n", irq);
    set_pending(gic_dist, irq, false);
    return 0;
}

static void
vgic_dist_irq_handler(struct irq* irq)
{
    struct device* vgic;
    vm_t* vm;

    vm = (vm_t*)irq->priv;
    vgic = vm_find_device_by_id(vm, DEV_VGIC_DIST);

    vgic_dist_set_pending_irq(vgic, vm, irq->virq);
}



static int
handle_vgic_dist_fault(struct device* d, vm_t* vm, fault_t* fault)
{
    struct gic_dist_map* gic_dist = vgic_priv_get_dist(d->priv);
    int offset;
    enum gic_dist_action act;
    uint32_t mask;
    uint32_t *reg;

    assert(d->priv);
    mask = fault_get_data_mask(fault);
    offset = fault->addr - d->pstart;

    reg = (uint32_t*)( (uintptr_t)gic_dist + offset );
    act = gic_dist_get_action(offset);

    assert(offset >= 0 && offset < d->size);

    /* Out of range */
    if (offset < 0 || offset >= sizeof(struct gic_dist_map)) {
        return ignore_fault(fault);

        /* Read fault */
    } else if (fault_is_read(fault)) {
        fault->data = *reg;
        return advance_fault(fault);
    } else {
        switch (act) {
        case ACTION_READONLY:
            return ignore_fault(fault);

        case ACTION_PASSTHROUGH:
            *reg = fault_emulate(fault, *reg);
            return advance_fault(fault);

        case ACTION_ENABLE:
            *reg = fault_emulate(fault, *reg);
            if (fault->data == 1) {
                vgic_dist_enable(d, vm);
            } else if (fault->data == 0) {
                vgic_dist_disable(d, vm);
            } else {
                assert(!"Unknown enable register encoding\n");
            }
            return advance_fault(fault);

        case ACTION_ENABLE_SET:
            /* Mask the data to write */
            fault->data &= mask;
            /* Mask bits that are already set */
            fault->data &= ~(*reg);
            while (fault->data) {
                int irq;
                irq = __builtin_ctz(fault->data);
                fault->data &= ~(1U << irq);
                irq += (offset - 0x100) * 8;
                vgic_dist_enable_irq(d, vm, irq);
            }
            return ignore_fault(fault);

        case ACTION_ENABLE_CLR:
            /* Mask the data to write */
            fault->data &= mask;
            /* Mask bits that are already clear */
            fault->data &= *reg;
            while (fault->data) {
                int irq;
                irq = __builtin_ctz(fault->data);
                fault->data &= ~(1U << irq);
                irq += (offset - 0x180) * 8;
                vgic_dist_disable_irq(d, vm, irq);
            }
            return ignore_fault(fault);

        case ACTION_PENDING_SET:
            /* Mask the data to write */
            fault->data &= mask;
            /* Mask bits that are already set */
            fault->data &= ~(*reg);
            while (fault->data) {
                int irq;
                irq = __builtin_ctz(fault->data);
                fault->data &= ~(1U << irq);
                irq += (offset - 0x200) * 8;
                vgic_dist_set_pending_irq(d, vm, irq);
            }
            return ignore_fault(fault);

        case ACTION_PENDING_CLR:
            /* Mask the data to write */
            fault->data &= mask;
            /* Mask bits that are already clear */
            fault->data &= *reg;
            while (fault->data) {
                int irq;
                irq = __builtin_ctz(fault->data);
                fault->data &= ~(1U << irq);
                irq += (offset - 0x280) * 8;
                vgic_dist_clr_pending_irq(d, vm, irq);
            }
            return ignore_fault(fault);

        case ACTION_SGI:
            assert(!"vgic SGI not implemented!\n");
            return ignore_fault(fault);

        case ACTION_UNKNOWN:
        default:
            printf("Unknown action on offset 0x%x\n", offset);
            return ignore_fault(fault);
        }
    }
    abandon_fault(fault);
    return -1;
}

static void vgic_dist_reset(struct device* d)
{
    struct gic_dist_map* gic_dist = vgic_priv_get_dist(d->priv);
    memset(gic_dist, 0, sizeof(*gic_dist));
    gic_dist->ic_type         = 0x0000fce7; /* RO */
    gic_dist->dist_ident      = 0x0200043b; /* RO */
    gic_dist->enable_set[0]   = 0x0000ffff; /* 16bit RO */
    gic_dist->enable_clr[0]   = 0x0000ffff; /* 16bit RO */
    gic_dist->config[0]       = 0xaaaaaaaa; /* RO */
    /* Reset value depends on GIC configuration */
    gic_dist->config[1]       = 0x55540000;
    gic_dist->config[2]       = 0x55555555;
    gic_dist->config[3]       = 0x55555555;
    gic_dist->config[4]       = 0x55555555;
    gic_dist->config[5]       = 0x55555555;
    gic_dist->config[6]       = 0x55555555;
    gic_dist->config[7]       = 0x55555555;
    gic_dist->config[8]       = 0x55555555;
    gic_dist->config[9]       = 0x55555555;
    gic_dist->config[10]      = 0x55555555;
    gic_dist->config[11]      = 0x55555555;
    gic_dist->config[12]      = 0x55555555;
    gic_dist->config[13]      = 0x55555555;
    gic_dist->config[14]      = 0x55555555;
    gic_dist->config[15]      = 0x55555555;
    /* identification */
    gic_dist->periph_id[4]    = 0x00000004; /* RO */
    gic_dist->periph_id[8]    = 0x00000090; /* RO */
    gic_dist->periph_id[9]    = 0x000000b4; /* RO */
    gic_dist->periph_id[10]   = 0x0000002b; /* RO */
    gic_dist->component_id[0] = 0x0000000d; /* RO */
    gic_dist->component_id[1] = 0x000000f0; /* RO */
    gic_dist->component_id[2] = 0x00000005; /* RO */
    gic_dist->component_id[3] = 0x000000b1; /* RO */
}


/*
 * 1) completely virtual the distributor
 * 2) remap vcpu to cpu. Full access
 */
int
vm_install_vgic(vm_t* vm)
{
    struct device dist, vcpu;
    struct vgic* vgic;
    void* addr;
    int err;

    /* Distributor */
    dist = dev_vgic_dist;
    vgic = malloc(sizeof(*vgic));
    if (!vgic) {
        assert(!"Unable to malloc memory for VGIC");
        return -1;
    }

    vgic->dist = map_emulated_device(vm, &dev_vgic_dist);
    assert(vgic->dist);
    if (vgic->dist == NULL) {
        return -1;
    }

    dist.priv = (void*)vgic;
    vgic_dist_reset(&dist);
    err = vm_add_device(vm, &dist);
    if (err) {
        free(dist.priv);
        return -1;
    }

    /* Remap VCPU to CPU */
    vcpu = dev_vgic_vcpu;
    addr = map_vm_device(vm, vcpu.pstart, dev_vgic_cpu.pstart, seL4_AllRights);
    assert(addr);
    if (!addr) {
        free(dist.priv);
        return -1;
    }
    vcpu.pstart = dev_vgic_cpu.pstart;
    err = vm_add_device(vm, &vcpu);
    assert(!err);
    if (err) {
        free(dist.priv);
        return -1;
    }
    return 0;
}

const struct device dev_vgic_dist = {
    .devid = DEV_VGIC_DIST,
    .name = "vgic.distributor",
    .pstart = GIC_DIST_PADDR,
    .size = 0x1000,
    .handle_page_fault = &handle_vgic_dist_fault,
    .priv = NULL,
};

const struct device dev_vgic_cpu = {
    .devid = DEV_VGIC_CPU,
    .name = "vgic.cpu_interface",
    .pstart = GIC_CPU_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL,
};

const struct device dev_vgic_vcpu = {
    .devid = DEV_VGIC_VCPU,
    .name = "vgic.vcpu_interface",
    .pstart = GIC_VCPU_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL,
};

