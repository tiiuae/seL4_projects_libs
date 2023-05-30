/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sel4vm/guest_ram.h>
#include <sel4vmmplatsupport/drivers/virtio_pci_emul.h>
#include <stdbool.h>

/**
 * The buffer size here should match BUFSIZE in vm/components/Init/src/virtio_con.c
 * and vm/components/VM_Arm/src/modules/virtio_con.c
 */
#define VUART_BUFLEN (0x1000 - 2 * sizeof(uint32_t))
static char buf[VUART_BUFLEN];

typedef struct console_virtio_emul_internal {
    struct virtio_console_driver driver;
    /* counter for console ports */
    unsigned int con_count;
    /* what queue did data come through? */
    int queue_num;
} console_internal_t;

typedef void(*tx_handler_fn_t)(virtio_emul_t *emul, int queue, void *buffer, unsigned int len);

/**
 * Takes data from the virtio console backend layer, adds them to the virtqueue, and notifies
 * the guest. This function assumes that the backend layer provides a ringbuffer.
 *
 * @see vm/components/Init/src/virtio_con.c or vm/components/VM_Arm/src/modules/virtio_con.c
 * for invariants of the ringbuffer.
 *
 * @param emul virtio device handler
 * @param queue queue number of the destination virtqueue
 * @param buf ringbuffer that contains the data to sent to the guest
 * @param head head of the ringbuffer
 * @param tail tail of the ringbuffer
 */
static void emul_con_rx_complete(virtio_emul_t *emul, int queue, char *buf, uint32_t head, uint32_t tail)
{
    if (head == tail) {
        return;
    }

    console_internal_t *con = emul->internal;
    vqueue_t *virtq = &emul->virtq;
    int i;
    struct vring *vring = &virtq->vring[queue];

    uint16_t guest_idx = ring_avail_idx(emul, vring);
    uint16_t idx = virtq->last_idx[queue];

    if (idx != guest_idx) {
        /* amount of the current descriptor written */
        size_t desc_written = 0;
        /* how much we have written of the current buffer */
        size_t buf_written = 0;
        /* len of the data that we need to write */
        size_t len = (tail > head) ? (tail - head) : (tail - head + VUART_BUFLEN);
        uint32_t current_head = head;

        uint16_t desc_head = ring_avail(emul, vring, idx);
        /* start walking the descriptors */
        struct vring_desc desc;
        uint16_t desc_idx = desc_head;
        do {
            desc = ring_desc(emul, vring, desc_idx);
            /* determine how much we can copy */
            uint32_t copy;
            copy = MIN(len - buf_written, VUART_BUFLEN - current_head);
            copy = MIN(copy, desc.len - desc_written);
            vm_guest_ram_write(emul->vm, (uintptr_t)desc.addr + desc_written, buf + current_head, copy);

            /* update amounts */
            desc_written += copy;
            buf_written += copy;
            current_head = (current_head + copy) % VUART_BUFLEN;
            /* see what's gone over */
            if (desc_written == desc.len) {
                if (!desc.flags & VRING_DESC_F_NEXT) {
                    /* descriptor chain is too short to hold the whole packet.
                     * just truncate */
                    break;
                }
                desc_idx = desc.next;
                desc_written = 0;
            }
        } while (buf_written != len);
        /* now put it in the used ring */
        struct vring_used_elem used_elem = {desc_head, buf_written};
        ring_used_add(emul, vring, used_elem);

        /* record that we've used this descriptor chain now */
        virtq->last_idx[queue]++;
        /* notify the guest that there is something in its used ring */
        con->driver.backend_fn.handleIRQ(con->driver.backend_fn.console_data);
    }
}


/**
 * Write to guest console attached to a port.
 *
 * The relationship between virtqueues and port numbers is as follows:
 * (0, 1): Port 0 RX and TX, respectively
 * (2, 3): Control messages RX and TX, used for setting up virtiocon
 * (4, 5): Port 1 RX and TX
 * (2+2*N, 2+2*N+1): Port N RX and TX
 */
void virtio_console_putchar(int port, virtio_emul_t *emul, char *buffer, uint32_t head, uint32_t tail)
{
    /* port -1 is for the control messages, all others as normal */
    int vq_num = 0;
    if (port > 0) {
        vq_num = CTL_RX_QUEUE + 2 * port;
    }
    emul_con_rx_complete(emul, vq_num, buffer, head, tail);
}

static virtio_console_callbacks_t emul_callbacks = {
    .emul_putchar = virtio_console_putchar
};

/* Write to port attached to queue number */
static void port_write(virtio_emul_t *emul, int queue, char *buffer, unsigned int len)
{
    console_internal_t *con = emul->internal;
    /* Get port number */
    int port;
    if (queue == 1) {
        port = 0;
    } else {
        port = queue / 2 - 1;
    }

    /* forward it */
    for (int i = 0; i < len; i++) {
        con->driver.backend_fn.backend_putchar(port, buffer[i]);
    }
}

static void handle_control_message(virtio_emul_t *emul, void *buffer)
{
    struct virtio_con_ctl *ctl_msg = (struct virtio_con_ctl *) buffer;

    console_internal_t *con = emul->internal;
    uint16_t event = ctl_msg->event;
    uint16_t value = ctl_msg->value;

    switch (event) {
    case VIRTIO_CON_DEVICE_READY:
    case VIRTIO_CON_PORT_READY:
        assert(value == 1);

        if (con->con_count != VIRTIO_CON_MAX_PORTS) {
            struct virtio_con_ctl out_msg = {
                con->con_count,
                VIRTIO_CON_PORT_ADD,
                0
            };
            emul_con_rx_complete(emul, CTL_RX_QUEUE, (char *) &out_msg, 0, sizeof(out_msg));

            con->con_count++;
        } else {
            /* Reset the console counter so we can use it to set ports as consoles */
            con->con_count = 0;

            /* Once we finish adding all ports, nominate port 0 as the console */
            struct virtio_con_ctl out_msg = {
                con->con_count,
                VIRTIO_CON_CON_PORT,
                1
            };
            con->con_count++;
            emul_con_rx_complete(emul, CTL_RX_QUEUE, (char *) &out_msg, 0, sizeof(out_msg));
        }

        break;
    case VIRTIO_CON_PORT_OPEN:
        assert(value == 1);
        /* Continue nominating the console ports */
        if (con->con_count != VIRTIO_CON_MAX_PORTS) {
            struct virtio_con_ctl out_msg = {
                con->con_count,
                VIRTIO_CON_CON_PORT,
                1
            };
            con->con_count++;
            emul_con_rx_complete(emul, CTL_RX_QUEUE, (char *) &out_msg, 0, sizeof(out_msg));
        }
        break;
    default:
        ZF_LOGE("Event %x in control TX queue unhandled", event);
    }
}

static void emul_con_notify_tx(virtio_emul_t *emul)
{
    console_internal_t *con = emul->internal;
    vqueue_t *virtq = &emul->virtq;


    struct vring *vring = &virtq->vring[con->queue_num];
    /* read the index */
    uint16_t guest_idx = ring_avail_idx(emul, vring);
    /* process what we can of the ring */

    uint16_t idx = virtq->last_idx[con->queue_num];
    while (idx != guest_idx) {
        /* read the head of the descriptor chain */
        uint16_t desc_head = ring_avail(emul, vring, idx);
        /* length of the final packet to deliver */
        uint32_t len = 0;
        /* start walking the descriptors */
        struct vring_desc desc;
        uint16_t desc_idx = desc_head;
        do {
            desc = ring_desc(emul, vring, desc_idx);

            vm_guest_ram_read(emul->vm, (uintptr_t)desc.addr, buf + len, desc.len);
            len += desc.len;
            desc_idx = desc.next;
        } while (desc.flags & VRING_DESC_F_NEXT);

        /* Handle data */
        if (con->queue_num == CTL_TX_QUEUE) {
            handle_control_message(emul, buf);
        } else {
            port_write(emul, con->queue_num, buf, len);
        }

        /* next */
        idx++;
        struct vring_used_elem used_elem = {desc_head, 0};
        ring_used_add(emul, &virtq->vring[con->queue_num], used_elem);
        con->driver.backend_fn.handleIRQ(con->driver.backend_fn.console_data);
    }
    /* update which parts of the ring we have processed */
    virtq->last_idx[con->queue_num] = idx;
}

bool console_device_emul_io_in(virtio_emul_t *emul, unsigned int offset, unsigned int size, unsigned int *result)
{
    bool handled = false;
    switch (offset) {
    case VIRTIO_PCI_HOST_FEATURES:
        handled = true;
        assert(size == 4);

        *result = BIT(VIRTIO_CON_F_MULTIPORT);
        break;
    case VIRTIO_CON_CFG_MAX_PORTS ... VIRTIO_CON_CFG_MAX_PORTS + 3:
        handled = true;

        /* Set VIRTIO_CON_MAX_PORTS in little-endian */
        if (offset == VIRTIO_CON_CFG_MAX_PORTS) {
            *result = VIRTIO_CON_MAX_PORTS;
        } else {
            *result = 0;
        }

        break;
    }
    return handled;
}

bool console_device_emul_io_out(virtio_emul_t *emul, unsigned int offset, unsigned int size, unsigned int value)
{
    bool handled = false;
    switch (offset) {
    case VIRTIO_PCI_GUEST_FEATURES:
        /* After sending the desired features to the driver, it responds
         * with all the features it can support. Here we make sure what we
         * want is provided by asserting */
        handled = true;
        assert(size == 4);
        assert(value == BIT(VIRTIO_CON_F_MULTIPORT));
        break;
    case VIRTIO_PCI_QUEUE_NOTIFY:
        handled = true;
        console_internal_t *con = emul->internal;
        con->queue_num = value;
        if (value % 2 == 0) {
            /* Ignore RX packets for now (see virtio_emul.c) */
        } else {
            /* Generic TX */
            emul->notify(emul);
        }
        break;
    }
    return handled;
}

void *console_virtio_emul_init(virtio_emul_t *emul, ps_io_ops_t io_ops, console_driver_init driver, void *config)
{
    console_internal_t *internal = NULL;
    int err;
    internal = calloc(1, sizeof(*internal));
    if (!internal) {
        goto error;
    }

    emul->device_io_in = console_device_emul_io_in;
    emul->device_io_out = console_device_emul_io_out;
    emul->notify = emul_con_notify_tx;
    internal->con_count = 0;
    internal->driver.emul_cb = emul_callbacks;

    err = driver(&internal->driver, io_ops, config);
    if (err) {
        ZF_LOGE("Failed to initialize driver");
        goto error;
    }

    return (void *)internal;
error:
    if (emul) {
        free(emul);
    }
    if (internal) {
        free(internal);
    }
    return NULL;
}
