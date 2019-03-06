/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#pragma once

#define VIRTIO_PCI_VENDOR_ID            0x1af4
#define VIRTIO_PCI_SUBSYSTEM_VENDOR_ID	0x1af4

#define VIRTIO_NET_PCI_DEVICE_ID		0x1000
#define VIRTIO_CONSOLE_PCI_DEVICE_ID	0x1003

#define VIRTIO_ID_NET		            1
#define VIRTIO_ID_CONSOLE	            3

#define VIRTIO_PCI_CLASS_NET			0x020000
#define VIRTIO_PCI_CLASS_CONSOLE		0x078000
