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

#pragma once

#include <stdint.h>
#include <pci/pci.h>

#include <sel4pci/pci.h>

#define PCI_BAR_OFFSET(b)	(offsetof(vmm_pci_device_def_t, bar##b))

/* Struct definition of a PCI device. This is used for emulating a device from
 * purely memory reads. This is not generally useful on its own, but provides
 * a nice skeleton */
typedef struct vmm_pci_device_def {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    uint32_t bar0;
    uint32_t bar1;
    uint32_t bar2;
    uint32_t bar3;
    uint32_t bar4;
    uint32_t bar5;
    uint32_t cardbus;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_rom;
    uint8_t caps_pointer;
    uint8_t reserved1;
    uint16_t reserved2;
    uint32_t reserved3;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint8_t min_grant;
    uint8_t max_latency;
    /* Now additional pointer to arbitrary capabilities */
    int caps_len;
    void *caps;
} PACKED vmm_pci_device_def_t;

typedef enum pci_mem_type {
    NON_MEM = 0,
    NON_PREFETCH_MEM,
    PREFETCH_MEM
} pci_mem_type_t;

typedef struct vmm_pci_bar {
    pci_mem_type_t mem_type;
    /* Address must be size aligned */
    uintptr_t address;
    size_t size_bits;
} vmm_pci_bar_t;

typedef struct pci_bar_emulation {
    vmm_pci_entry_t passthrough;
    int num_bars;
    vmm_pci_bar_t bars[6];
    uint32_t bar_writes[6];
} pci_bar_emulation_t;

typedef struct pci_irq_emulation {
    vmm_pci_entry_t passthrough;
    int irq;
} pci_irq_emulation_t;

typedef struct pci_passthrough_device {
    /* The address on the host system of this device */
    vmm_pci_address_t addr;
    /* Ops for accessing config space */
    vmm_pci_config_t config;
} pci_passthrough_device_t;

typedef struct pci_cap_emulation {
    vmm_pci_entry_t passthrough;
    int num_caps;
    uint8_t *caps;
    int num_ignore;
    uint8_t *ignore_start;
    uint8_t *ignore_end;
} pci_cap_emulation_t;

/* Helper write function that just ignores any writes */
int vmm_pci_entry_ignore_write(void *cookie, int offset, int size, uint32_t value);

/* Read and write methods for a memory device */
int vmm_pci_mem_device_read(void *cookie, int offset, int size, uint32_t *result);
int vmm_pci_mem_device_write(void *cookie, int offset, int size, uint32_t value);

void define_pci_host_bridge(vmm_pci_device_def_t *bridge);

/* Construct a pure passthrough device based on the real PCI. This is almost always useless as
 * you will almost certainly want to rebase io memory */
vmm_pci_entry_t vmm_pci_create_passthrough(vmm_pci_address_t addr, vmm_pci_config_t config);

/* Bar read/write emulation, rest passed on */
vmm_pci_entry_t vmm_pci_create_bar_emulation(vmm_pci_entry_t existing, int num_bars, vmm_pci_bar_t *bars);

/* Interrupt read/write emulation, rest passed on */
vmm_pci_entry_t vmm_pci_create_irq_emulation(vmm_pci_entry_t existing, int irq);

/* Capability space emulation. Takes list of addresses to use to form a capability
 * linked list, as well as a ranges of the capability space that should be
 * directly disallowed. Assumes a type 0 device.
 */
vmm_pci_entry_t vmm_pci_create_cap_emulation(vmm_pci_entry_t existing, int num_caps, uint8_t *caps, int num_ranges, uint8_t *range_starts, uint8_t *range_ends);

/* Finds the MSI capabilities and uses vmm_pci_create_cap_emulation to remove them */
vmm_pci_entry_t vmm_pci_no_msi_cap_emulation(vmm_pci_entry_t existing);
