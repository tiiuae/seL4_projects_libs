#pragma once

#ifdef DEBUG_DIST
#define DDIST(...) do{ printf("VDIST: "); printf(__VA_ARGS__); }while(0)
#else
#define DDIST(...) do{}while(0)
#endif

/* Memory map for GIC distributer */
struct vgic_dist_map {
    uint32_t enable;                                    /* 0x000 */
    uint32_t ic_type;                                   /* 0x004 */
    uint32_t dist_ident;                                /* 0x008 */
    uint32_t res1[29];                                  /* [0x00C, 0x080) */

    uint32_t irq_group[32];                             /* [0x080, 0x100) */

    uint32_t enable_set[32];                            /* [0x100, 0x180) */
    uint32_t enable_clr[32];                            /* [0x180, 0x200) */
    uint32_t pending_set[32];                           /* [0x200, 0x280) */
    uint32_t pending_clr[32];                           /* [0x280, 0x300) */
    uint32_t active[32];                                /* [0x300, 0x380) */
    uint32_t active_clr[32];                            /* [0x380, 0x400) */

    uint32_t priority[255];                             /* [0x400, 0x7FC) */
    uint32_t res3;                                      /* 0x7FC */

    uint32_t targets[255];                              /* [0x800, 0xBFC) */
    uint32_t res4;                                      /* 0xBFC */

    uint32_t config[64];                                /* [0xC00, 0xD00) */

    uint32_t spi[32];                                   /* [0xD00, 0xD80) */
    uint32_t res5[20];                                  /* [0xD80, 0xDD0) */
    uint32_t res6;                                      /* 0xDD0 */
    uint32_t legacy_int;                                /* 0xDD4 */
    uint32_t res7[2];                                   /* [0xDD8, 0xDE0) */
    uint32_t match_d;                                   /* 0xDE0 */
    uint32_t enable_d;                                  /* 0xDE4 */
    uint32_t res8[70];                                  /* [0xDE8, 0xF00) */

    uint32_t sgi_control;                               /* 0xF00 */
    uint32_t res9[3];                                   /* [0xF04, 0xF10) */

    uint32_t _sgi_pending_clr[4];                       /* [0xF10, 0xF20) */
    uint32_t _sgi_pending_set[4];                       /* [0xF20, 0xF30) */
    uint32_t res10[40];                                 /* [0xF30, 0xFC0) */

    uint32_t periph_id[12];                             /* [0xFC0, 0xFF0) */
    uint32_t component_id[4];                           /* [0xFF0, 0xFFF] */

    uint32_t irq_group0[CONFIG_MAX_NUM_NODES];
    uint32_t enable_set0[CONFIG_MAX_NUM_NODES];
    uint32_t enable_clr0[CONFIG_MAX_NUM_NODES];
    uint32_t pending_set0[CONFIG_MAX_NUM_NODES];
    uint32_t pending_clr0[CONFIG_MAX_NUM_NODES];
    uint32_t active0[CONFIG_MAX_NUM_NODES];
    uint32_t active_clr0[CONFIG_MAX_NUM_NODES];
    uint32_t priority0[CONFIG_MAX_NUM_NODES][8];
    uint32_t targets0[CONFIG_MAX_NUM_NODES][8];
    uint32_t sgi_pending_clr[CONFIG_MAX_NUM_NODES][4];
    uint32_t sgi_pending_set[CONFIG_MAX_NUM_NODES][4];
};

enum gic_dist_action {
    ACTION_SGI_IRQ_GROUP = 0,
    ACTION_SGI_ENABLE_SET,
    ACTION_SGI_ENABLE_CLR,
    ACTION_SGI_PENDING_SET,
    ACTION_SGI_PENDING_CLR,
    ACTION_SGI_ACTIVE,
    ACTION_SGI_ACTIVE_CLR,
    ACTION_SGI_PRIORITY,
    ACTION_SGI_TARGETS,
    ACTION_NUM_SGI_ACTIONS,

    ACTION_ENABLE,
    ACTION_ENABLE_SET,
    ACTION_ENABLE_CLR,
    ACTION_PENDING_SET,
    ACTION_PENDING_CLR,
    ACTION_SGI_CTRL,

    ACTION_READONLY,
    ACTION_PASSTHROUGH
};
