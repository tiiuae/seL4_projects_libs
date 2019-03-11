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

/*vm exits related with io instructions*/

#include <stdio.h>
#include <stdlib.h>

#include <sel4utils/util.h>
#include <sel4vmm-core/util/io.h>

static int io_port_compare_by_range(const void *pkey, const void *pelem) {
    unsigned int key = (unsigned int)(uintptr_t)pkey;
    const ioport_entry_t *entry = (const ioport_entry_t*)pelem;
    const ioport_range_t *elem = &entry->range;
    if (key < elem->start) {
        return -1;
    }
    if (key > elem->end) {
        return 1;
    }
    return 0;
}

static int io_port_compare_by_start (const void *a, const void *b) {
    const ioport_entry_t *a_entry = (const ioport_entry_t*)a;
    const ioport_range_t *a_range = &a_entry->range;
    const ioport_entry_t *b_entry = (const ioport_entry_t*)b;
    const ioport_range_t *b_range = &b_entry->range;
    return a_range->start - b_range->start;
}

static ioport_entry_t *search_port(vmm_io_port_list_t *io_port, unsigned int port_no) {
    return (ioport_entry_t*)bsearch((void*)(uintptr_t)port_no, io_port->ioports, io_port->num_ioports, sizeof(ioport_entry_t), io_port_compare_by_range);
}

/* Debug helper function for port no. */
static const char* vmm_debug_io_portno_desc(vmm_io_port_list_t *io_port, int port_no) {
    ioport_entry_t *port = search_port(io_port, port_no);
    return port ? port->interface.desc : "Unknown IO Port";
}

/* IO execution handler. */
int emulate_io_handler(vmm_io_port_list_t *io_port, unsigned int port_no, bool is_in, size_t size, unsigned int *data) {

    unsigned int value;

    ZF_LOGI(4, "vm exit io request: in %d  port no 0x%x (%s) size %d\n",
            is_in, port_no, vmm_debug_io_portno_desc(io_port, port_no), size);

    ioport_entry_t *port = search_port(io_port, port_no);
    if (!port) {
        static int last_port = -1;
        if (last_port != port_no) {
            ZF_LOGE("vm exit io request: WARNING - ignoring unsupported ioport 0x%x (%s)\n", port_no,
                    vmm_debug_io_portno_desc(io_port, port_no));
            last_port = port_no;
        }
        return 0;
    }

    int ret = 0;
    if (is_in) {
        ret = port->interface.port_in(port->interface.cookie, port_no, size, data);
    } else {
        ret = port->interface.port_out(port->interface.cookie, port_no, size, *data);
    }

    if (ret) {
        ZF_LOGE("vm exit io request: handler returned error.");
        ZF_LOGE("vm exit io ERROR: string %d  in %d rep %d  port no 0x%x (%s) size %d", 0,
                is_in, 0, port_no, vmm_debug_io_portno_desc(io_port, port_no), size);
        return -1;
    }

    return 0;
}

static int add_io_port_range(vmm_io_port_list_t *io_list, ioport_entry_t port) {
    /* ensure this range does not overlap */
    for (int i = 0; i < io_list->num_ioports; i++) {
        if (io_list->ioports[i].range.end > port.range.start && io_list->ioports[i].range.start < port.range.end) {
            ZF_LOGE("Requested ioport range 0x%x-0x%x for %s overlaps with existing range 0x%x-0x%x for %s",
                port.range.start, port.range.end, port.interface.desc ? port.interface.desc : "Unknown IO Port", io_list->ioports[i].range.start, io_list->ioports[i].range.end, io_list->ioports[i].interface.desc ? io_list->ioports[i].interface.desc : "Unknown IO Port");
            return -1;
        }
    }
    /* grow the array */
    io_list->ioports = realloc(io_list->ioports, sizeof(ioport_entry_t) * (io_list->num_ioports + 1));
    assert(io_list->ioports);
    /* add the new entry */
    io_list->ioports[io_list->num_ioports] = port;
    io_list->num_ioports++;
    /* sort */
    qsort(io_list->ioports, io_list->num_ioports, sizeof(ioport_entry_t), io_port_compare_by_start);
    return 0;
}

int vmm_io_port_add_passthrough(vmm_io_port_list_t *io_list, ioport_range_t io_range, ioport_interface_t io_interface) {
    return add_io_port_range(io_list, (ioport_entry_t){io_range, io_interface, IOPORT_PASSTHROUGH});
}

//int vmm_io_port_add_handler(vmm_io_port_list_t *io_list, uint16_t start, uint16_t end, void *cookie, ioport_in_fn port_in, ioport_out_fn port_out, const char *desc) {
/* Add an io port range for emulation */
int vmm_io_port_add_handler(vmm_io_port_list_t *io_list, ioport_range_t io_range, ioport_interface_t io_interface) {
    return add_io_port_range(io_list, (ioport_entry_t){io_range, io_interface, IOPORT_EMULATED});
}

int vmm_io_port_init(vmm_io_port_list_t *io_list) {
    io_list->num_ioports = 0;
    io_list->ioports = malloc(0);
    assert(io_list->ioports);
    return 0;
}
