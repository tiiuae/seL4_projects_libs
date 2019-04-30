/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#include <sel4arm-vmm/plat/device_map.h>
#include <sel4arm-vmm/devices.h>


const struct device dev_usb1 = {
    .devid = DEV_CUSTOM,
    .name = "usb3",
    .pstart = USB1_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};


const struct device dev_usb3 = {
    .devid = DEV_CUSTOM,
    .name = "usb3",
    .pstart = USB3_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};

const struct device dev_sdmmc = {
    .devid = DEV_CUSTOM,
    .name = "usb3",
    .pstart = SDMMC_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};

const struct device dev_rtc_kbc_pmc = {
    .devid = DEV_CUSTOM,
    .name = "rtc_kbc_pmc",
    .pstart = RTC_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};

const struct device dev_data_memory = {
    .devid = DEV_CUSTOM,
    .name = "data_memory",
    .pstart = DATA_MEMORY_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};

const struct device dev_exception_vectors = {
    .devid = DEV_CUSTOM,
    .name = "exception_vectors",
    .pstart = EXCEPTION_VECTORS_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};

const struct device dev_system_registers = {
    .devid = DEV_CUSTOM,
    .name = "system_registers",
    .pstart = SYSTEM_REGISTERS_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};

const struct device dev_ictlr = {
    .devid = DEV_CUSTOM,
    .name = "ictlr",
    .pstart = ICTLR_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};

const struct device dev_apb_misc = {
    .devid = DEV_CUSTOM,
    .name = "apb_misc",
    .pstart = APB_MISC_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};

const struct device dev_fuse = {
    .devid = DEV_CUSTOM,
    .name = "fuse",
    .pstart = FUSE_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};

const struct device dev_gpios = {
    .devid = DEV_CUSTOM,
    .name = "gpios",
    .pstart = GPIOS_PADDR,
    .size = 0x1000,
    .handle_page_fault = NULL,
    .priv = NULL
};
