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

#include <sel4vm/guest_ram.h>

struct guest_mem_touch_params {
    void *data;
    size_t size;
    size_t offset;
    uintptr_t current_addr;
    vm_t *vm;
    ram_touch_callback_fn touch_fn;
};

/* Helpers for use with touch below */
int vm_guest_ram_read_callback(vm_t *vm, uintptr_t addr, void *vaddr, size_t size, size_t offset, void *buf) {
    memcpy(buf, vaddr, size);
    return 0;
}

int vm_guest_ram_write_callback(vm_t *vm, uintptr_t addr, void *vaddr, size_t size, size_t offset, void *buf) {
    memcpy(vaddr, buf, size);
    return 0;
}

static int touch_access_callback(void *access_addr, void *vaddr, void *cookie) {
    struct guest_mem_touch_params *guest_touch = (struct guest_mem_touch_params *)cookie;
    uintptr_t vmm_addr = (uintptr_t)vaddr;
    uintptr_t vm_addr = (uintptr_t)access_addr;
    return guest_touch->touch_fn(guest_touch->vm, vm_addr,
            (void *)(vmm_addr + (guest_touch->current_addr - vm_addr)),
            guest_touch->size, guest_touch->offset, guest_touch->data);
}

int vm_ram_touch(vm_t *vm, uintptr_t addr, size_t size, ram_touch_callback_fn touch_callback, void *cookie) {
    struct guest_mem_touch_params access_cookie;
    uintptr_t current_addr;
    uintptr_t next_addr;
    uintptr_t end_addr = (uintptr_t)(addr + size);
    access_cookie.touch_fn = touch_callback;
    access_cookie.data = cookie;
    access_cookie.vm = vm;
    for (current_addr = addr; current_addr < end_addr; current_addr = next_addr) {
        uintptr_t current_aligned = PAGE_ALIGN_4K(current_addr);
        uintptr_t next_page_start = current_aligned + PAGE_SIZE_4K;
        next_addr = MIN(end_addr, next_page_start);
        access_cookie.size = next_addr - current_addr;
        access_cookie.offset = current_addr - addr;
        access_cookie.current_addr = current_addr;
        int result = vspace_access_page_with_callback(&vm->mem.vm_vspace, &vm->mem.vmm_vspace, (void *)current_aligned,
                vm->mem.page_size, seL4_AllRights, 1, touch_access_callback, &access_cookie);
        if (result) {
            return result;
        }
    }
    return 0;
}
