<!--
     Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)

     SPDX-License-Identifier: CC-BY-SA-4.0
-->

## Interface `pci.h`

This interface presents a VMM PCI Driver. This manages the host's PCI devices, and handles guest OS PCI config space
read & writes.

### Brief content:

**Functions**:

> [`vmm_pci_init(space, flags)`](#function-vmm_pci_initspace-flags)

> [`vmm_pci_add_entry(space, entry, addr)`](#function-vmm_pci_add_entryspace-entry-addr)

> [`make_addr_reg_from_config(space, conf, addr, reg)`](#function-make_addr_reg_from_configspace-conf-addr-reg)

> [`vmm_pci_config_size(space)`](#function-vmm_pci_config_sizespace)

> [`vmm_pci_is_ecam(space)`](#function-vmm_pci_is_ecamspace)

> [`find_device(self, addr)`](#function-find_deviceself-addr)



**Structs**:

> [`vmm_pci_address`](#struct-vmm_pci_address)

> [`vmm_pci_entry`](#struct-vmm_pci_entry)

> [`vmm_pci_sapce`](#struct-vmm_pci_sapce)


## Functions

The interface `pci.h` defines the following functions.

### Function `vmm_pci_init(space, flags)`

Initialize PCI space

**Parameters:**

- `space {vmm_pci_space_t **}`: Pointer to PCI space being initialised
- `flags {vmm_pci_flags_t}`: Features to be configured for the PCI space

**Returns:**

- 0 on success, -1 on error

Back to [interface description](#module-pcih).

### Function `vmm_pci_add_entry(space, entry, addr)`

Add a PCI entry. Optionally reports where it is located

**Parameters:**

- `space {vmm_pci_space_t *}`: PCI space handle
- `entry {vmm_pci_entry_t}`: PCI entry being addr
- `addr {vmm_pci_addr_t *}`: Resulting PCI address where entry gets located

**Returns:**

- 0 on success, -1 on error

Back to [interface description](#module-pcih).

### Function `make_addr_reg_from_config(space, conf, addr, reg)`

Convert config to compatible PCI configuration space address

**Parameters:**

- `space {vmm_pci_space_t *}`: PCI space handle
- `conf {uint32_t}`: Configuration value to convert to pci address
- `addr {vmm_pci_address_t *}`: Resulting PCI address
- `reg {uint8_t *}`: Resulting register value

**Returns:**

No return

Back to [interface description](#module-pcih).

### Function `vmm_pci_config_size(space)`

Get the size of PCI configuration space

**Parameters:**

- `space {vmm_pci_space_t *}`: PCI space handle

**Returns:**

- The size of PCI configuration space

Back to [interface description](#module-pcih).

### Function `vmm_pci_is_ecam(space)`

Query whether Enhanced Configuration Access Mechanism (ECAM) is being used

**Parameters:**

- `space {vmm_pci_space_t *}`: PCI space handle

**Returns:**

- true if ecam, otherwise false

Back to [interface description](#module-pcih).

### Function `find_device(self, addr)`

Find PCI device given a PCI address (Bus/Dev/Func)

**Parameters:**

- `self {vmm_pci_space_t *}`: PCI space handle
- `addr {vmm_pci_address_t}`: PCI address of device

**Returns:**

- NULL on error, otherwise pointer to registered pci entry

Back to [interface description](#module-pcih).


## Structs

The interface `pci.h` defines the following structs.

### Struct `vmm_pci_address`

Represents a PCI address by Bus/Device/Function

**Elements:**

- `bus {uint8_t}`: Bus value
- `dev {uint8_t}`: Device value
- `fun {uint8_t}`: Function value

Back to [interface description](#module-pcih).

### Struct `vmm_pci_entry`

Abstracts the virtual PCI device. This is is inserted into the virtual PCI configuration space

**Elements:**

- `cookie {void *}`: User supplied cookie to pass onto callback functions
- `ioread {int *(void *cookie, int offset, int size, uint32_t *result)}`: Configuration space read callback
- `iowrite {int *(void *cookie, int offset, int size, uint32_t value)}`: Configuration space write callback

Back to [interface description](#module-pcih).

### Struct `vmm_pci_space`

Represents a single host virtual PCI space
                                        This only supports one bus at the moment.

**Elements:**

- `bus {vmm_pci_entry_t *}`: The PCI bus, representing 32 devices, each of which has 8 functions
- `conf_port_addr {uint32_t}`: The current config address for IO port emulation
- `flags {vmm_pci_flags_t}`: The flags indicating the features supported by the PCI bus

Back to [interface description](#module-pcih).


Back to [top](#).

