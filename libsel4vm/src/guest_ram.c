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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sel4/sel4.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_ram.h>
#include <sel4vm/guest_memory.h>
#include <sel4vm/guest_memory_util.h>

struct guest_mem_touch_params {
    void *data;
    size_t size;
    size_t offset;
    uintptr_t current_addr;
    vm_t *vm;
    ram_touch_callback_fn touch_fn;
};

static int push_guest_ram_region(vm_mem_t *guest_memory, uintptr_t start, size_t size, int allocated) {
    int last_region = guest_memory->num_ram_regions;
    guest_memory->ram_regions = realloc(guest_memory->ram_regions, sizeof(vm_ram_region_t) * (last_region + 1));
    assert(guest_memory->ram_regions);
    if (size == 0) return -1;

    guest_memory->ram_regions[last_region].start = start;
    guest_memory->ram_regions[last_region].size = size;
    guest_memory->ram_regions[last_region].allocated = allocated;
    guest_memory->num_ram_regions++;
    return 0;
}

static int ram_region_cmp(const void *a, const void *b) {
    const vm_ram_region_t *aa = a;
    const vm_ram_region_t *bb = b;
    return aa->start - bb->start;
}

static void sort_guest_ram_regions(vm_mem_t *guest_memory) {
    qsort(guest_memory->ram_regions, guest_memory->num_ram_regions, sizeof(vm_ram_region_t), ram_region_cmp);
}

static void guest_ram_remove_region(vm_mem_t *guest_memory, int region) {
    if (region >= guest_memory->num_ram_regions) {
        return;
    }
    guest_memory->num_ram_regions--;
    memmove(&guest_memory->ram_regions[region], &guest_memory->ram_regions[region + 1], sizeof(vm_ram_region_t) * (guest_memory->num_ram_regions - region));
    /* realloc it smaller */
    guest_memory->ram_regions = realloc(guest_memory->ram_regions, sizeof(vm_ram_region_t) * guest_memory->num_ram_regions);
}

static void collapse_guest_ram_regions(vm_mem_t *guest_memory) {
    int i;
    for (i = 1; i < guest_memory->num_ram_regions;) {
        /* Only collapse regions with the same allocation flag that are contiguous */
        if (guest_memory->ram_regions[i - 1].allocated == guest_memory->ram_regions[i].allocated &&
            guest_memory->ram_regions[i - 1].start + guest_memory->ram_regions[i - 1].size == guest_memory->ram_regions[i].start) {

            guest_memory->ram_regions[i - 1].size += guest_memory->ram_regions[i].size;
            guest_ram_remove_region(guest_memory, i);
        } else {
            /* We are satisified that this entry cannot be merged. So now we
             * move onto the next one */
            i++;
        }
    }
}

static int expand_guest_ram_region(vm_t *vm, uintptr_t start, size_t bytes) {
    int err;
    vm_mem_t *guest_memory = &vm->mem;
    /* blindly put a new region at the end */
    err = push_guest_ram_region(guest_memory, start, bytes, 0);
    if (err) {
        ZF_LOGE("Failed to expand guest ram region");
        return err;
    }
    /* sort the region we just added */
    sort_guest_ram_regions(guest_memory);
    /* collapse any contiguous regions */
    collapse_guest_ram_regions(guest_memory);
    return 0;
}

static memory_fault_result_t default_ram_fault_callback(vm_t *vm, uintptr_t fault_addr, size_t fault_length,
        void *cookie, guest_memory_arch_data_t arch_data) {
    /* We don't handle RAM faults by default unless the callback is specifically overrided, hence we fail here */
    ZF_LOGE("ERROR: UNHANDLED RAM FAULT");
    return FAULT_ERROR;
}

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

int vm_ram_find_largest_free_region(vm_t *vm, uintptr_t *addr, size_t *size) {
    vm_mem_t *guest_memory = &vm->mem;
    int largest = -1;
    int i;
    /* find a first region */
    for (i = 0; i < guest_memory->num_ram_regions && largest == -1; i++) {
        if (!guest_memory->ram_regions[i].allocated) {
            largest = i;
        }
    }
    if (largest == -1) {
        ZF_LOGE("Failed to find free region");
        return -1;
    }
    for (i++; i < guest_memory->num_ram_regions; i++) {
        if (!guest_memory->ram_regions[i].allocated &&
            guest_memory->ram_regions[i].size > guest_memory->ram_regions[largest].size) {
            largest = i;
        }
    }
    *addr = guest_memory->ram_regions[largest].start;
    *size = guest_memory->ram_regions[largest].size;
    return 0;
}

void vm_ram_mark_allocated(vm_t *vm, uintptr_t start, size_t bytes) {
    vm_mem_t *guest_memory = &vm->mem;
    /* Find the region */
    int i;
    int region = -1;
    for (i = 0; i < guest_memory->num_ram_regions; i++) {
        if (guest_memory->ram_regions[i].start <= start &&
            guest_memory->ram_regions[i].start + guest_memory->ram_regions[i].size >= start + bytes) {
            region = i;
            break;
        }
    }
    if (region == -1 || guest_memory->ram_regions[region].allocated) {
        return;
    }
    /* Remove the region */
    vm_ram_region_t r = guest_memory->ram_regions[region];
    guest_ram_remove_region(guest_memory, region);
    /* Split the region into three pieces and add them */
    push_guest_ram_region(guest_memory, r.start, start - r.start, 0);
    push_guest_ram_region(guest_memory, start, bytes, 1);
    push_guest_ram_region(guest_memory, start + bytes, r.size - bytes - (start - r.start), 0);
    /* sort and collapse */
    sort_guest_ram_regions(guest_memory);
    collapse_guest_ram_regions(guest_memory);
}

uintptr_t vm_ram_allocate(vm_t *vm, size_t bytes) {
    vm_mem_t *guest_memory = &vm->mem;
    for (int i = 0; i < guest_memory->num_ram_regions; i++) {
        if (!guest_memory->ram_regions[i].allocated && guest_memory->ram_regions[i].size >= bytes) {
            uintptr_t addr = guest_memory->ram_regions[i].start;
            vm_ram_mark_allocated(vm, addr, bytes);
            return addr;
        }
    }
    ZF_LOGE("Failed to allocate %zu bytes of guest RAM", bytes);
    return 0;
}

static int map_ram_reservation(vm_t *vm, vm_memory_reservation_t *ram_reservation, bool one_to_one) {
    memory_map_iterator_fn map_iterator;
    int err;
    if (one_to_one) {
        err = map_maybe_device_reservation(vm, ram_reservation);
    } else {
        err = map_ut_allocman_reservation(vm, ram_reservation);
    }
    if (err) {
        ZF_LOGE("Failed to map new ram reservation");
        return -1;
    }
}

uintptr_t vm_ram_register(vm_t *vm, size_t bytes, bool one_to_one) {
    vm_memory_reservation_t *ram_reservation;
    int err;
    uintptr_t base_addr;

    ram_reservation = vm_reserve_anon_memory(vm, bytes, default_ram_fault_callback, NULL, &base_addr);
    if (!ram_reservation) {
        ZF_LOGE("Unable to reserve ram region of size 0x%x", bytes);
        return 0;
    }
    err = map_ram_reservation(vm, ram_reservation, one_to_one);
    if (err) {
        vm_free_reserved_memory(vm, ram_reservation);
        return 0;
    }
    err = expand_guest_ram_region(vm, base_addr, bytes);
    if (err) {
        ZF_LOGE("Failed to register new ram region");
        vm_free_reserved_memory(vm, ram_reservation);
        return 0;
    }

    return base_addr;
}

int vm_ram_register_at(vm_t *vm, uintptr_t start, size_t bytes, bool one_to_one) {
    vm_memory_reservation_t *ram_reservation;
    int err;

    ram_reservation = vm_reserve_memory_at(vm, start, bytes, default_ram_fault_callback,
            NULL);
    if (!ram_reservation) {
        ZF_LOGE("Unable to reserve ram region at addr 0x%x of size 0x%x", start, bytes);
        return 0;
    }
    err = map_ram_reservation(vm, ram_reservation, one_to_one);
    if (err) {
        vm_free_reserved_memory(vm, ram_reservation);
        return 0;
    }
    err = expand_guest_ram_region(vm, start, bytes);
    if (err) {
        ZF_LOGE("Failed to register new ram region");
        vm_free_reserved_memory(vm, ram_reservation);
        return 0;
    }
    return 0;
}

void vm_ram_free(vm_t *vm, uintptr_t start, size_t bytes) {
    return;
}
