/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
/*
 * This component controls and maintains the GIC for the VM.
 * IRQs must be registered at init time with vm_virq_new(...)
 * This function creates and registers an IRQ data structure which will be used for IRQ maintenance
 * b) ENABLING: When the VM enables the IRQ, it checks the pending flag for the VM.
 *   - If the IRQ is not pending, we either
 *        1) have not received an IRQ so it is still enabled in seL4
 *        2) have received an IRQ, but ignored it because the VM had disabled it.
 *     In either case, we simply ACK the IRQ with seL4. In case 1), the IRQ will come straight through,
       in case 2), we have ACKed an IRQ that was not yet pending anyway.
 *   - If the IRQ is already pending, we can assume that the VM has yet to ACK the IRQ and take no further
 *     action.
 *   Transitions: b->c
 * c) PIRQ: When an IRQ is received from seL4, seL4 disables the IRQ and sends an async message. When the VMM
 *    receives the message.
 *   - If the IRQ is enabled, we set the pending flag in the VM and inject the appropriate IRQ
 *     leading to state d)
 *   - If the IRQ is disabled, the VMM takes no further action, leading to state b)
 *   Transitions: (enabled)? c->d :  c->b
 * d) When the VM acknowledges the IRQ, an exception is raised and delivered to the VMM. When the VMM
 *    receives the exception, it clears the pending flag and acks the IRQ with seL4, leading back to state c)
 *    Transition: d->c
 * g) When/if the VM disables the IRQ, we may still have an IRQ resident in the GIC. We allow
 *    this IRQ to be delivered to the VM, but subsequent IRQs will not be delivered as seen by state c)
 *    Transitions g->c
 *
 *   NOTE: There is a big assumption that the VM will not manually manipulate our pending flags and
 *         destroy our state. The affects of this will be an IRQ that is never acknowledged and hence,
 *         will never occur again.
 */

#include "vgic.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <utils/arith.h>
#include <vka/vka.h>
#include <vka/capops.h>

#include <sel4vm/vm.h>
#include <sel4vm/gen_config.h>
#include <sel4vm/guest_vm.h>
#include <sel4vm/boot.h>
#include <sel4vm/guest_memory.h>
#include <sel4vm/guest_irq_controller.h>

#include "vm.h"
#include "../fault.h"

//#define DEBUG_IRQ
//#define DEBUG_DIST

#ifdef DEBUG_IRQ
#define DIRQ(...) do{ printf("VDIST: "); printf(__VA_ARGS__); }while(0)
#else
#define DIRQ(...) do{}while(0)
#endif

#ifdef DEBUG_DIST
#define DDIST(...) do{ printf("VDIST: "); printf(__VA_ARGS__); }while(0)
#else
#define DDIST(...) do{}while(0)
#endif


/* FIXME these should be defined in a way that is friendlier to extension. */
#if defined(CONFIG_PLAT_EXYNOS5)
#define GIC_PADDR   0x10480000
#elif defined(CONFIG_PLAT_TK1) || defined(CONFIG_PLAT_TX1)
#define GIC_PADDR   0x50040000
#elif defined(CONFIG_PLAT_TX2)
#define GIC_PADDR   0x03880000
#elif defined(CONFIG_PLAT_QEMU_ARM_VIRT)
#define GIC_PADDR   0x8000000
#else
#error "Unsupported platform for GIC"
#endif

#ifdef CONFIG_PLAT_QEMU_ARM_VIRT
#define GIC_DIST_PADDR       (GIC_PADDR)
#define GIC_CPU_PADDR        (GIC_PADDR + 0x00010000)
#define GIC_VCPU_CNTR_PADDR  (GIC_PADDR + 0x00030000)
#define GIC_VCPU_PADDR       (GIC_PADDR + 0x00040000)
#else
#define GIC_DIST_PADDR       (GIC_PADDR + 0x1000)
#define GIC_CPU_PADDR        (GIC_PADDR + 0x2000)
#define GIC_VCPU_CNTR_PADDR  (GIC_PADDR + 0x4000)
#define GIC_VCPU_PADDR       (GIC_PADDR + 0x6000)
#endif

#define MAX_VIRQS   200

#define IRQ_IDX(irq) ((irq) / 32)
#define IRQ_BIT(irq) (1U << ((irq) % 32))

#define not_pending(...) !is_pending(__VA_ARGS__)
#define not_active(...)  !is_active(__VA_ARGS__)
#define not_enabled(...) !is_enabled(__VA_ARGS__)

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

struct virq_handle {
    int virq;
    irq_ack_fn_t ack;
    void *token;
};

typedef struct virq_handle* virq_handle_t;

static inline void virq_ack(vm_t *vm, struct virq_handle *irq)
{
    irq->ack(vm, irq->virq, irq->token);
}

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

#define MAX_LR_OVERFLOW 64
#define LR_OF_NEXT(_i) (((_i) == MAX_LR_OVERFLOW - 1) ? 0 : ((_i) + 1))

struct lr_of {
    struct virq_handle irqs[MAX_LR_OVERFLOW]; /* circular buffer */
    size_t head;
    size_t tail;
    bool full;
};

typedef struct vgic {
/// Mirrors the vcpu list registers
    struct virq_handle *irq[63];
/// IRQs that would not fit in the vcpu list registers
    struct lr_of lr_overflow;
/// Complete set of virtual irqs
    struct virq_handle *virqs[MAX_VIRQS];
/// Virtual distributer registers
    struct gic_dist_map *dist;
} vgic_t;

static struct vgic_dist_device *vgic_dist;

static struct virq_handle* virq_find_irq_data(struct vgic* vgic, int virq) {
    int i;
    for (i = 0; i < MAX_VIRQS; i++) {
        if (vgic->virqs[i] && vgic->virqs[i]->virq == virq) {
            return vgic->virqs[i];
        }
    }
    return NULL;
}

static int virq_add(vgic_t *vgic, struct virq_handle *virq_data)
{
    int i;
    for (i = 0; i < MAX_VIRQS; i++) {
        if (vgic->virqs[i] == NULL) {
            vgic->virqs[i] = virq_data;
            return 0;
        }
    }
    return -1;
}

static int virq_init(vgic_t *vgic)
{
    memset(vgic->irq, 0, sizeof(vgic->irq));
    memset(vgic->virqs, 0, sizeof(vgic->virqs));
    vgic->lr_overflow.head = 0;
    vgic->lr_overflow.tail = 0;
    vgic->lr_overflow.full = false;
    return 0;
}

static inline struct vgic* vgic_device_get_vgic(struct vgic_dist_device* d) {
    assert(d);
    assert(d->priv);
    return (vgic_t *)d->priv;
}

static inline struct gic_dist_map* vgic_priv_get_dist(struct vgic_dist_device* d) {
    assert(d);
    assert(d->priv);
    return vgic_device_get_vgic(d)->dist;
}

static inline struct virq_handle** vgic_priv_get_lr(struct vgic_dist_device* d) {
    assert(d);
    assert(d->priv);
    return vgic_device_get_vgic(d)->irq;
}



static inline void set_pending(struct gic_dist_map *gic_dist, int irq, int v)
{
    if (v) {
        gic_dist->pending_set[IRQ_IDX(irq)] |= IRQ_BIT(irq);
        gic_dist->pending_clr[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->pending_set[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
        gic_dist->pending_clr[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline int is_pending(struct gic_dist_map *gic_dist, int irq)
{
    return !!(gic_dist->pending_set[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static inline void set_enable(struct gic_dist_map *gic_dist, int irq, int v)
{
    if (v) {
        gic_dist->enable_set[IRQ_IDX(irq)] |= IRQ_BIT(irq);
        gic_dist->enable_clr[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->enable_set[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
        gic_dist->enable_clr[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline int is_enabled(struct gic_dist_map *gic_dist, int irq)
{
    return !!(gic_dist->enable_set[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static inline void set_active(struct gic_dist_map *gic_dist, int irq, int v)
{
    if (v) {
        gic_dist->active[IRQ_IDX(irq)] |= IRQ_BIT(irq);
    } else {
        gic_dist->active[IRQ_IDX(irq)] &= ~IRQ_BIT(irq);
    }
}

static inline int is_active(struct gic_dist_map *gic_dist, int irq)
{
    return !!(gic_dist->active[IRQ_IDX(irq)] & IRQ_BIT(irq));
}

static int list_size = 0;

static int
vgic_vcpu_inject_irq(struct vgic_dist_device* d, vm_t *vm, struct virq_handle *irq)
{
    vgic_t *vgic;
    int err;
    int i;

    vgic = vgic_device_get_vgic(d);

    seL4_CPtr vcpu;
    vcpu = vm->vcpus[BOOT_VCPU]->vcpu.cptr;
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
        int idx = vgic->lr_overflow.tail;
        ZF_LOGF_IF(vgic->lr_overflow.full, "too many overflow irqs");
        vgic->lr_overflow.irqs[idx] = *irq;
        vgic->lr_overflow.full = (vgic->lr_overflow.head == LR_OF_NEXT(vgic->lr_overflow.tail));
        if (!vgic->lr_overflow.full) {
            vgic->lr_overflow.tail = LR_OF_NEXT(idx);
        }
        return 0;
    }
}

int handle_vgic_maintenance(vm_t *vm, int idx)
{
#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
    vm->arch.lock();
#endif //CONCONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT

    /* STATE d) */
    struct gic_dist_map* gic_dist;
    struct virq_handle** lr;

    assert(vgic_dist);
    gic_dist = vgic_priv_get_dist(vgic_dist);
    lr = vgic_priv_get_lr(vgic_dist);
    assert(lr[idx]);

    /* Clear pending */
    DIRQ("Maintenance IRQ %d\n", lr[idx]->virq);
    set_pending(gic_dist, lr[idx]->virq, false);
    virq_ack(vm, lr[idx]);

    /* Check the overflow list for pending IRQs */
    lr[idx] = NULL;
    vgic_t *vgic = vgic_device_get_vgic(vgic_dist);
    /* copy tail, as vgic_vcpu_inject_irq can mutate it, and we do
     * not want to process any new overflow irqs */
    size_t tail = vgic->lr_overflow.tail;
    for (size_t i = vgic->lr_overflow.head; i != tail; i = LR_OF_NEXT(i)) {
        if (vgic_vcpu_inject_irq(vgic_dist, vm, &vgic->lr_overflow.irqs[i]) == 0) {
            vgic->lr_overflow.head = LR_OF_NEXT(i);
            vgic->lr_overflow.full = (vgic->lr_overflow.head == LR_OF_NEXT(vgic->lr_overflow.tail));
        } else {
            break;
        }
    }
#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
    vm->arch.unlock();
#endif //CONCONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT

    return 0;
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
vgic_dist_enable(struct vgic_dist_device* d, vm_t* vm)
{
    struct gic_dist_map *gic_dist = vgic_priv_get_dist(d);
    DDIST("enabling gic distributer\n");
    gic_dist->enable = 1;
    return 0;
}

static int
vgic_dist_disable(struct vgic_dist_device* d, vm_t* vm)
{
    struct gic_dist_map *gic_dist = vgic_priv_get_dist(d);
    DDIST("disabling gic distributer\n");
    gic_dist->enable = 0;
    return 0;
}

static int
vgic_dist_enable_irq(struct vgic_dist_device* d, vm_t* vm, int irq)
{
    struct gic_dist_map *gic_dist;
    struct virq_handle *virq_data;
    vgic_t *vgic;
    gic_dist = vgic_priv_get_dist(d);
    vgic = vgic_device_get_vgic(d);
    DDIST("enabling irq %d\n", irq);
    set_enable(gic_dist, irq, true);
    virq_data = virq_find_irq_data(vgic, irq);
    if (virq_data) {
        /* STATE b) */
        if (not_pending(gic_dist, virq_data->virq)) {
            virq_ack(vm, virq_data);
        }
    } else {
        DDIST("enabled irq %d has no handle", irq);
    }
    return 0;
}

static int
vgic_dist_disable_irq(struct vgic_dist_device* d, vm_t* vm, int irq)
{
    /* STATE g) */
    struct gic_dist_map *gic_dist = vgic_priv_get_dist(d);
    if (irq >= 16) {
        DDIST("disabling irq %d\n", irq);
        set_enable(gic_dist, irq, false);
    }
    return 0;
}

static int
vgic_dist_set_pending_irq(struct vgic_dist_device* d, vm_t* vm, int irq)
{
#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
    vm->arch.lock();
#endif //CONCONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT

    /* STATE c) */
    struct gic_dist_map *gic_dist;
    vgic_t *vgic;
    struct virq_handle *virq_data;

    gic_dist = vgic_priv_get_dist(d);
    vgic = vgic_device_get_vgic(d);

    virq_data = virq_find_irq_data(vgic, irq);
    /* If it is enables, inject the IRQ */
    if (virq_data && gic_dist->enable && is_enabled(gic_dist, irq)) {
        int err;
        DDIST("Pending set: Inject IRQ from pending set (%d)\n", irq);

        set_pending(gic_dist, virq_data->virq, true);
        err = vgic_vcpu_inject_irq(d, vm, virq_data);
        assert(!err);

#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
        vm->arch.unlock();
#endif //CONCONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
        return err;
    } else {
        /* No further action */
        DDIST("IRQ not enabled (%d)\n", irq);
    }

#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
    vm->arch.unlock();
#endif //CONCONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT

    return 0;
}

static int
vgic_dist_clr_pending_irq(struct vgic_dist_device* d, vm_t* vm, int irq)
{
    struct gic_dist_map *gic_dist = vgic_priv_get_dist(d);
    DDIST("clr pending irq %d\n", irq);
    set_pending(gic_dist, irq, false);
    return 0;
}

static memory_fault_result_t
handle_vgic_dist_fault(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr, size_t fault_length,
        void *cookie)
{
    int err;
    struct vgic_dist_device* d;
    fault_t* fault;
    struct gic_dist_map* gic_dist;
    int offset;
    enum gic_dist_action act;
    uint32_t mask;
    uint32_t *reg;

    fault = vcpu->vcpu_arch.fault;
    d = (struct vgic_dist_device *)cookie;
    gic_dist = vgic_priv_get_dist(d);
    mask = fault_get_data_mask(fault);
    offset = fault_get_address(fault) - d->pstart;

    offset = ALIGN_DOWN(offset, sizeof(uint32_t));

    reg = (uint32_t *)((uintptr_t)gic_dist + offset);
    act = gic_dist_get_action(offset);

    assert(offset >= 0 && offset < d->size);

    /* Out of range */
    if (offset < 0 || offset >= sizeof(struct gic_dist_map)) {
        DDIST("offset out of range %x %x\n", offset, sizeof(struct gic_dist_map));
        err = ignore_fault(fault);
        /* Read fault */
    } else if (fault_is_read(fault)) {
        fault_set_data(fault, *reg);
        err = ignore_fault(fault);
    } else {
        uint32_t data;
        switch (act) {
        case ACTION_READONLY:
            err = ignore_fault(fault);
            break;
        case ACTION_PASSTHROUGH:
            *reg = fault_emulate(fault, *reg);
            err = advance_fault(fault);
            break;
        case ACTION_ENABLE:
            *reg = fault_emulate(fault, *reg);
            data = fault_get_data(fault);
            if (data == 1) {
                vgic_dist_enable(d, vm);
            } else if (data == 0) {
                vgic_dist_disable(d, vm);
            } else {
                assert(!"Unknown enable register encoding\n");
            }
            err = advance_fault(fault);
            break;
        case ACTION_ENABLE_SET:
            data = fault_get_data(fault);
            /* Mask the data to write */
            data &= mask;
            /* Mask bits that are already set */
            data &= ~(*reg);
            while (data) {
                int irq;
                irq = CTZ(data);
                data &= ~(1U << irq);
                irq += (offset - 0x100) * 8;
                vgic_dist_enable_irq(d, vm, irq);
            }
            err = ignore_fault(fault);
            break;
        case ACTION_ENABLE_CLR:
            data = fault_get_data(fault);
            /* Mask the data to write */
            data &= mask;
            /* Mask bits that are already clear */
            data &= *reg;
            while (data) {
                int irq;
                irq = CTZ(data);
                data &= ~(1U << irq);
                irq += (offset - 0x180) * 8;
                vgic_dist_disable_irq(d, vm, irq);
            }
            err = ignore_fault(fault);
            break;
        case ACTION_PENDING_SET:
            data = fault_get_data(fault);
            /* Mask the data to write */
            data &= mask;
            /* Mask bits that are already set */
            data &= ~(*reg);
            while (data) {
                int irq;
                irq = CTZ(data);
                data &= ~(1U << irq);
                irq += (offset - 0x200) * 8;
                vgic_dist_set_pending_irq(d, vm, irq);
            }
            err = ignore_fault(fault);
            break;
        case ACTION_PENDING_CLR:
            data = fault_get_data(fault);
            /* Mask the data to write */
            data &= mask;
            /* Mask bits that are already clear */
            data &= *reg;
            while (data) {
                int irq;
                irq = CTZ(data);
                data &= ~(1U << irq);
                irq += (offset - 0x280) * 8;
                vgic_dist_clr_pending_irq(d, vm, irq);
            }
            err = ignore_fault(fault);
            break;
        case ACTION_SGI:
            assert(!"vgic SGI not implemented!\n");
            err = ignore_fault(fault);
            break;
        case ACTION_UNKNOWN:
        default:
            DDIST("Unknown action on offset 0x%x\n", offset);
            err = ignore_fault(fault);
        }
    }
    if (!err) {
        return FAULT_HANDLED;
    }
    return FAULT_ERROR;
}

static void vgic_dist_reset(struct vgic_dist_device* d)
{
    struct gic_dist_map *gic_dist;
    gic_dist = vgic_priv_get_dist(d);
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

    /* This tells Linux that all IRQs are routed to CPU0.
     * When we eventually support multiple vCPUs per guest,
     * this will need to be updated.
     */
    for (int i = 0; i < ARRAY_SIZE(gic_dist->targets); i++) {
        gic_dist->targets[i] = 0x01010101;
    }
}

int vm_register_irq(vm_t *vm, int irq, irq_ack_fn_t ack_fn, void *cookie)
{
    struct virq_handle* virq_data;
    struct vgic* vgic;
    int err;

    vgic = vgic_device_get_vgic(vgic_dist);
    assert(vgic);

    virq_data = malloc(sizeof(*virq_data));
    if (!virq_data) {
        return -1;
    }
    virq_data->virq = irq;
    virq_data->token = cookie;
    virq_data->ack = ack_fn;
    err = virq_add(vgic, virq_data);
    if (err) {
        free(virq_data);
        return -1;
    }
    return 0;
}

int vm_inject_irq(vm_t *vm, int irq)
{
    // vm->lock();

    DIRQ("VM received IRQ %d\n", virq->virq);

    vgic_dist_set_pending_irq(vgic_dist, vm, irq);

    if (!fault_handled(vm->vcpus[BOOT_VCPU]->vcpu_arch.fault) && fault_is_wfi(vm->vcpus[BOOT_VCPU]->vcpu_arch.fault)) {
        ignore_fault(vm->vcpus[BOOT_VCPU]->vcpu_arch.fault);
    }

    // vm->unlock();

    return 0;
}

static memory_fault_result_t
handle_vgic_vcpu_fault(vm_t *vm, vm_vcpu_t *vcpu, uintptr_t fault_addr, size_t fault_length,
        void *cookie) {
    /* We shouldn't fault on the vgic vcpu region as it should be mapped in
     * with all rights */
    return FAULT_ERROR;
}

struct vgic_dist_frame_cookie {
    vka_object_t frame;
    cspacepath_t mapped_frame;
    vm_t *vm;
    vm_memory_reservation_t *reservation;
};

static vm_frame_t vgic_dist_frame_iterator(uintptr_t addr, void *cookie) {
    cspacepath_t return_frame;
    vm_t *vm;
    int page_size;
    vm_frame_t frame_result = { seL4_CapNull, seL4_NoRights, 0, 0 };

    struct vgic_dist_frame_cookie *dist_cookie = (struct vgic_dist_frame_cookie *)cookie;
    if (!dist_cookie) {
        return frame_result;
    }
    vm = dist_cookie->vm;
    page_size = seL4_PageBits;

    int ret = vka_cspace_alloc_path(vm->vka, &return_frame);
    if (ret) {
        ZF_LOGE("Failed to allocate cspace path from device frame");
        return frame_result;
    }
    ret = vka_cnode_copy(&return_frame, &dist_cookie->mapped_frame, seL4_CanRead);
    if (ret) {
        ZF_LOGE("Failed to cnode_copy for device frame");
        vka_cspace_free_path(vm->vka, return_frame);
        return frame_result;
    }
    frame_result.cptr = return_frame.capPtr;
    frame_result.rights = seL4_CanRead;
    frame_result.vaddr = GIC_DIST_PADDR;
    frame_result.size_bits = page_size;
    return frame_result;
}

static void *create_vgic_distributor_frame(vm_t *vm) {
    int err;
    struct vgic_dist_frame_cookie *cookie;
    int page_size = seL4_PageBits;
    vspace_t *vmm_vspace = &vm->mem.vmm_vspace;
    ps_io_ops_t *ops = vm->io_ops;

    err = ps_calloc(&ops->malloc_ops, 1, sizeof(struct vgic_dist_frame_cookie), (void **)&cookie);
    if (err) {
        ZF_LOGE("Failed to create allocated vm frame: Unable to allocate cookie");
        return NULL;
    }

    /* Reserve emulated vgic frame */
    cookie->reservation = vm_reserve_memory_at(vm, GIC_DIST_PADDR, PAGE_SIZE_4K,
            handle_vgic_dist_fault, (void *)vgic_dist);
    if (!cookie->reservation) {
        ZF_LOGE("Failed to create emulate vgic dist frame");
        ps_free(&ops->malloc_ops, sizeof(struct vgic_dist_frame_cookie), (void **)&cookie);
        return NULL;
    }

    err = vka_alloc_frame(vm->vka, page_size, &cookie->frame);
    if (err) {
        ZF_LOGE("Failed vka_alloc_frame for vgic dist frame");
        vm_free_reserved_memory(vm, cookie->reservation);
        ps_free(&ops->malloc_ops, sizeof(struct vgic_dist_frame_cookie), (void **)&cookie);
        return NULL;
    }
    vka_cspace_make_path(vm->vka, cookie->frame.cptr, &cookie->mapped_frame);
    void *vgic_dist_addr = vspace_map_pages(vmm_vspace, &cookie->mapped_frame.capPtr,
                                  NULL, seL4_AllRights, 1, page_size, 0);
    if (!vgic_dist_addr) {
        ZF_LOGE("Failed to map vgic dist frame into vmm vspace");
        vka_free_object(vm->vka, &cookie->frame);
        vm_free_reserved_memory(vm, cookie->reservation);
        ps_free(&ops->malloc_ops, sizeof(struct vgic_dist_frame_cookie), (void **)&cookie);
        return NULL;
    }

    cookie->vm = vm;

    /* Map the vgic dist frame */
    err = vm_map_reservation(vm, cookie->reservation, vgic_dist_frame_iterator, (void *)cookie);
    if (err) {
        ZF_LOGE("Failed to map allocated frame into vm");
        vka_free_object(vm->vka, &cookie->frame);
        vm_free_reserved_memory(vm, cookie->reservation);
        ps_free(&ops->malloc_ops, sizeof(struct vgic_dist_frame_cookie), (void **)&cookie);
        return NULL;
    }

    return vgic_dist_addr;
}

static vm_frame_t vgic_vcpu_iterator(uintptr_t addr, void *cookie) {
    int err;
    cspacepath_t frame;
    vm_frame_t frame_result = { seL4_CapNull, seL4_NoRights, 0, 0 };
    vm_t *vm = (vm_t *)cookie;

    err = vka_cspace_alloc_path(vm->vka, &frame);
    if (err) {
        printf("Failed to allocate cslot for vgic vcpu\n");
        return frame_result;
    }
    seL4_Word vka_cookie;
    err = vka_utspace_alloc_at(vm->vka, &frame, kobject_get_type(KOBJECT_FRAME, 12), 12, GIC_VCPU_PADDR, &vka_cookie);
    if (err) {
        err = simple_get_frame_cap(vm->simple, (void*)GIC_VCPU_PADDR, 12, &frame);
        if (err) {
            ZF_LOGE("Failed to find device cap for vgic vcpu\n");
            return frame_result;
        }
    }
    frame_result.cptr = frame.capPtr;
    frame_result.rights = seL4_AllRights;
    frame_result.vaddr = GIC_CPU_PADDR;
    frame_result.size_bits = seL4_PageBits;
    return frame_result;
}

/*
 * 1) completely virtual the distributor
 * 2) remap vcpu to cpu. Full access
 */
int vm_install_vgic(vm_t *vm)
{
    struct vgic* vgic;
    void* addr;
    int err;

    vgic = malloc(sizeof(*vgic));
    if (!vgic) {
        assert(!"Unable to malloc memory for VGIC");
        return -1;
    }
    err = virq_init(vgic);
    if (err) {
        free(vgic);
        return -1;
    }

    /* Distributor */
    vgic_dist = (struct vgic_dist_device *)malloc(sizeof(struct vgic_dist_device));
    if (!vgic_dist) {
        return -1;
    }
    memcpy(vgic_dist, &dev_vgic_dist, sizeof(struct vgic_dist_device));

    vgic->dist = create_vgic_distributor_frame(vm);

    assert(vgic->dist);
    if (vgic->dist == NULL) {
        return -1;
    }
    vgic_dist->priv = (void*)vgic;
    vgic_dist_reset(vgic_dist);

    /* Remap VCPU to CPU */
    vm_memory_reservation_t *vgic_vcpu_reservation = vm_reserve_memory_at(vm, GIC_CPU_PADDR,
            0x1000, handle_vgic_vcpu_fault, NULL);
    err = vm_map_reservation(vm, vgic_vcpu_reservation, vgic_vcpu_iterator, (void *)vm);
    if (err) {
        free(vgic_dist->priv);
        return -1;
    }

    return 0;
}

int vm_vgic_maintenance_handler(vm_vcpu_t *vcpu) {
    int idx;
    int err;
    idx = seL4_GetMR(seL4_UnknownSyscall_ARG0);
    /* Currently not handling spurious IRQs */
    assert(idx >= 0);

    err = handle_vgic_maintenance(vcpu->vm, idx);
    if (!err) {
        seL4_MessageInfo_t reply;
        reply = seL4_MessageInfo_new(0, 0, 0, 0);
        seL4_Reply(reply);
    }
    return VM_EXIT_HANDLED;
}

const struct vgic_dist_device dev_vgic_dist = {
    .pstart = GIC_DIST_PADDR,
    .size = 0x1000,
    .priv = NULL,
};
