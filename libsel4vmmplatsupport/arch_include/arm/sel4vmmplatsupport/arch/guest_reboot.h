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

typedef int (*reboot_hook_fn)(vm_t* vm, void *token);

typedef struct reboot_hook {
    reboot_hook_fn fn;
    void *token;
} reboot_hook_t;

typedef struct reboot_hooks_list {
    reboot_hook_t *rb_hooks;
    size_t nhooks;
} reboot_hooks_list_t;

/**
 * Initialise state of a given reboot hooks list
 * @param[in] rb_hooks_list     Handle to reboot hooks list
 * @return                      0 for success, otherwise -1 for error
 */
int vmm_init_reboot_hooks_list(reboot_hooks_list_t *rb_hooks_list);

/**
 * Register a reboot callback within a given reboot hooks list
 * @param[in] rb_hooks_list     Handle to reboot hooks list
 * @param[in] hook              Reboot callback to be invoked when list is processed
 * @param[in] token             Cookie passed to reboot callback when invoked
 * @return                      0 for success, otherwise -1 for error
 */
int vmm_register_reboot_callback(reboot_hooks_list_t *rb_hooks_list, reboot_hook_fn hook, void *token);

/**
*  Process the reboot hooks registered in a reboot hooks list
 * @param[in] vm                Handle to vm - passed onto reboot callback
 * @param[in] rb_hooks_list     Handle to reboot hooks list
 * @return                      0 for success, otherwise -1 for error
 */
int vmm_process_reboot_callbacks(vm_t *vm, reboot_hooks_list_t *rb_hooks_list);
