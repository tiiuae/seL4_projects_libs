/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _LIBGA_BGA_H_
#define _LIBGA_BGA_H_

#include <stdint.h>

/* More information about the BGA device itself is available from
 * http://wiki.osdev.org/Bochs_VBE_Extensions.
 */

/* Opaque pointer as a handle to the device. */
typedef struct bga *bga_p;

/* Initialise the driver. Returns 0 on success.
 *  io_port_capability - An IO port capability with permission to read and
 *      write the relevant IO ports for the BGA (0x1ce and 0x1cf).
 *  fb_vaddr - The starting address of where the user has mapped in the frame
 *      buffer of the BGA.
 */

/* Initialise the driver. Returns NULL on failure.
 *  framebuffer - A pointer to the device frame mapped into your address space.
 *  ioport_write - Callback for writing to an IO port.
 *  ioport_read - Callback for reading from an IO port.
 */
bga_p bga_init(void *framebuffer,
    void (*ioport_write)(uint16_t port, uint16_t value),
    uint16_t (*ioport_read)(uint16_t port));

/* Destroy and clean up resources associated with an initialised device.
 * Returns 0 on success, non-zero on failure.
 */
int bga_destroy(bga_p device);

/* Get the version number of this device. */
uint16_t bga_version(bga_p device);

/* Set the mode of the device. Returns 0 on success. On error, the device might
 * be left in an invalid state.
 *  width - The width of the display in pixels.
 *  height - The height of the display in pixels.
 *  bpp - The bits per pixel.
 */
int bga_set_mode(bga_p device, unsigned int width, unsigned int height,
    unsigned int bpp);

/* Set the value of the pixel at (x, y). How value is interpreted is dependent
 * on the current bits per pixel configuration. To know how to pass this value
 * correctly, consult http://wiki.osdev.org/Bochs_VBE_Extensions. Returns 0 on
 * success.
 *
 * This function is mainly for drawing simple output to the screen. For
 * anything performance-critical you will want to use the raw frame buffer
 * access below.
 *
 * XXX: This function has not been tested with BPPs other than 24.
 */
int bga_set_pixel(bga_p device, unsigned int x, unsigned int y, char *value);

    /* With raw access enabled you can directly read from and write to the
     * frame buffer. Note that you will need to know the frame buffer
     * dimensions and current configuration to write to it correctly.
     */

/* Get a pointer to the frame buffer. You can output to the screen by directly
 * writing into this buffer. To do this correctly you will have to consult the
 * Bochs documentation for formatting details.
 */
void *bga_get_framebuffer(bga_p device);

#endif /* !_LIBBGA_BGA_H_ */
