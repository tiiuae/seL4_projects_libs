#pragma once

/* Memory map for GIC Redistributor Registers for control and physical LPI's */
struct vgic_rdist_map {
    uint32_t    ctlr;           /* 0x0000 */
    uint32_t    iidr;           /* 0x0004 */
    uint64_t    typer;          /* 0x008 */
    uint32_t    res0;           /* 0x0010 */
    uint32_t    waker;          /* 0x0014 */
    uint32_t    res1[21];       /* 0x0018 */
    uint64_t    propbaser;      /* 0x0070 */
    uint64_t    pendbaser;      /* 0x0078 */
    uint32_t    res2[16340];    /* 0x0080 */
    uint32_t    pidr4;          /* 0xFFD0 */
    uint32_t    pidr5;          /* 0xFFD4 */
    uint32_t    pidr6;          /* 0xFFD8 */
    uint32_t    pidr7;          /* 0xFFDC */
    uint32_t    pidr0;          /* 0xFFE0 */
    uint32_t    pidr1;          /* 0xFFE4 */
    uint32_t    pidr2;          /* 0xFFE8 */
    uint32_t    pidr3;          /* 0xFFEC */
    uint32_t    cidr0;          /* 0xFFF0 */
    uint32_t    cidr1;          /* 0xFFF4 */
    uint32_t    cidr2;          /* 0xFFF8 */
    uint32_t    cidr3;          /* 0xFFFC */
};

/* Memory map for the GIC Redistributor Registers for the SGI and PPI's */
struct vgic_rdist_sgi_ppi_map {
    uint32_t    res0[32];       /* 0x0000 */
    uint32_t    igroup[32];     /* 0x0080 */
    uint32_t    isenable[32];   /* 0x0100 */
    uint32_t    icenable[32];   /* 0x0180 */
    uint32_t    ispend[32];     /* 0x0200 */
    uint32_t    icpend[32];     /* 0x0280 */
    uint32_t    isactive[32];   /* 0x0300 */
    uint32_t    icactive[32];   /* 0x0380 */
    uint32_t    ipriorityrn[8]; /* 0x0400 */
    uint32_t    res1[504];      /* 0x0420 */
    uint32_t    icfgrn_ro;      /* 0x0C00 */
    uint32_t    icfgrn_rw;      /* 0x0C04 */
    uint32_t    res2[62];       /* 0x0C08 */
    uint32_t    igrpmod[64];    /* 0x0D00 */
    uint32_t    nsac;           /* 0x0E00 */
    uint32_t    res11[11391];   /* 0x0E04 */
    uint32_t    miscstatsr;     /* 0xC000 */
    uint32_t    res3[31];       /* 0xC004 */
    uint32_t    ppisr;          /* 0xC080 */
    uint32_t    res4[4062];     /* 0xC084 */
};
