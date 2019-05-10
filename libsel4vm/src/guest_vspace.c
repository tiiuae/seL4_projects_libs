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

#include <autoconf.h>
#include <sel4vm/gen_config.h>

#include <sel4vm/guest_vspace.h>
#include <sel4utils/vspace.h>
#include <sel4utils/vspace_internal.h>
#include <vka/capops.h>

#if defined(CONFIG_ARM_SMMU) || defined(CONFIG_IOMMU)
typedef struct guest_iospace {
    seL4_CPtr iospace;
    struct sel4utils_alloc_data iospace_vspace_data;
    vspace_t iospace_vspace;
} guest_iospace_t;
#endif

typedef struct guest_vspace {
    /* We abuse struct ordering and this member MUST be the first
     * thing in the struct */
    struct sel4utils_alloc_data vspace_data;
    /* additional vspace to place all mappings into. We will maintain
     * a translation between this and the guest */
    vspace_t vmm_vspace;
    /* use a vspace implementation as a sparse data structure to track
     * the translation from guest to vmm */
    struct sel4utils_alloc_data translation_vspace_data;
    vspace_t translation_vspace;
#if defined(CONFIG_ARM_SMU) || defined(CONFIG_IOMMU)
    /* debug flag for checking if we add io spaces late */
    int done_mapping;
    int num_iospaces;
    guest_iospace_t **iospaces;
} guest_vspace_t;

static int guest_vspace_map(vspace_t *vspace, seL4_CPtr cap, void *vaddr, seL4_CapRights_t rights,
                            int cacheable, size_t size_bits)
{
    struct sel4utils_alloc_data *data = get_alloc_data(vspace);
    int error;
    /* this type cast works because the alloc data was at the start of the struct
     * so it has the same address.
     * This conversion is guaranteed to work by the C standard */
    guest_vspace_t *guest_vspace = (guest_vspace_t*) data;
    /* perform the guest mapping */
    error = guest_vspace_map_page_arch(vspace, cap, vaddr, rights, cacheable, size_bits);
    if (error) {
        return error;
    }
    /* duplicate the cap so we can do a mapping */
    cspacepath_t orig_path;
    vka_cspace_make_path(guest_vspace->vspace_data.vka, cap, &orig_path);
    cspacepath_t new_path;
    error = vka_cspace_alloc_path(guest_vspace->vspace_data.vka, &new_path);
    if (error) {
        ZF_LOGE("Failed to allocate cslot to duplicate frame cap");
        return error;
    }
    error = vka_cnode_copy(&new_path, &orig_path, seL4_AllRights);
    assert(error == seL4_NoError);
    /* perform the regular mapping */
    void *vmm_vaddr = vspace_map_pages(&guest_vspace->vmm_vspace, &new_path.capPtr, NULL, seL4_AllRights, 1, size_bits,
                                       cacheable);
    if (!vmm_vaddr) {
        ZF_LOGE("Failed to map into VMM vspace");
        return -1;
    }
    /* add translation information. give dummy cap value of 42 as it cannot be zero
     * but we really just want to store information in the cookie */
    error = update_entries(&guest_vspace->translation_vspace, (uintptr_t)vaddr, new_path.capPtr, size_bits, (uintptr_t)vmm_vaddr);
    if (error){
        ZF_LOGE("Failed to add translation information");
        return error;
    }
#if defined(CONFIG_ARM_SMMU) || defined(CONFIG_IOMMU)
    /* set the mapping bit */
    guest_vspace->done_mapping = 1;
    /* map into all the io spaces */
    for (int i = 0; i < guest_vspace->num_iospaces; i++) {
        error = vka_cspace_alloc_path(guest_vspace->vspace_data.vka, &new_path);
        if (error) {
            ZF_LOGE("Failed to allocate cslot to duplicate frame cap");
            return error;
        }
        error = vka_cnode_copy(&new_path, &orig_path, seL4_AllRights);

        guest_iospace_t *guest_iospace = guest_vspace->iospaces[i];

        assert(error == seL4_NoError);
        error = sel4utils_map_iospace_page(guest_vspace->vspace_data.vka, guest_iospace->iospace,
                                           new_path.capPtr, (uintptr_t)vaddr, rights, 1,
                                           size_bits, NULL, NULL);
        if (error) {
            ZF_LOGE("Failed to map page into iospace");
            return error;
        }

        /* Store the slot of the frame cap copy in a vspace so they can be looked up and
         * freed when this address gets unmapped. */
        error = update_entries(&guest_iospace->iospace_vspace, (uintptr_t)vaddr, new_path.capPtr, size_bits, 0 /* cookie */);
        if (error) {
            ZF_LOGE("Failed to add iospace mapping information");
            return error;
        }
    }
#else
    (void)guest_vspace;
#endif
    return 0;
}

void guest_vspace_unmap(vspace_t *vspace, void *vaddr, size_t num_pages, size_t size_bits, vka_t *vka)
{
    struct sel4utils_alloc_data *data = get_alloc_data(vspace);
    guest_vspace_t *guest_vspace = (guest_vspace_t *) data;

    int error;

    /* Unmap pages from PT.
     * vaddr is a guest physical address.
     * This can be done in a single call as mappings are contiguous in this vspace. */
    sel4utils_unmap_pages(vspace, vaddr, num_pages, size_bits, vka);

    /* Each page must be unmapped individually from the vmm vspace, as mappings are not
     * necessarily host-virtually contiguous. */
    size_t page_size = BIT(size_bits);

    for (int i = 0; i < num_pages; i++) {

        void *page_vaddr = (void *)(vaddr + i * page_size);

        // look up vaddr in vmm vspace by consulting entry in translation vspace
        void *vmm_vaddr = (void *)vspace_get_cookie(&guest_vspace->translation_vspace, page_vaddr);

        // remove mapping from vmm vspace
        vspace_unmap_pages(&guest_vspace->vmm_vspace, vmm_vaddr, 1 /* num pages */, size_bits, vka);

        // remove mapping from translation vspace
        error = clear_entries(&guest_vspace->translation_vspace, (uintptr_t)page_vaddr, size_bits);

        if (error) {
            ZF_LOGE("Failed to clear translation information");
            return;
        }

#if defined(CONFIG_ARM_SMMU) || defined(CONFIG_IOMMU)
        /* Unmap the vaddr from each iospace, freeing the cslots used to store the
         * copy of the frame cap. */
        for (int i = 0; i < guest_vspace->num_iospaces; i++) {
            guest_iospace_t *guest_iospace = guest_vspace->iospaces[i];
            seL4_CPtr iospace_frame_cap_copy = vspace_get_cap(&guest_iospace->iospace_vspace, page_vaddr);

            error = seL4_ARCH_Page_Unmap(iospace_frame_cap_copy);
            if (error) {
                ZF_LOGE("Failed to unmap page from iospace");
                return;
            }

            cspacepath_t path;
            vka_cspace_make_path(guest_vspace->vspace_data.vka, iospace_frame_cap_copy, &path);
            error = vka_cnode_delete(&path);
            if (error) {
                ZF_LOGE("Failed to delete frame cap copy");
                return;
            }

            vka_cspace_free(guest_vspace->vspace_data.vka, iospace_frame_cap_copy);

            error = clear_entries(&guest_iospace->iospace_vspace, (uintptr_t)page_vaddr, size_bits);
            if (error) {
                ZF_LOGE("Failed to clear iospace mapping information");
                return;
            }
        }
#endif
    }
}

#if defined(CONFIG_ARM_SMMU) || defined(CONFIG_IOMMU)
int vm_guest_vspace_add_iospace(vspace_t *loader, vspace_t *vspace, seL4_CPtr iospace) {
    struct sel4utils_alloc_data *data = get_alloc_data(vspace);
    guest_vspace_t *guest_vspace = (guest_vspace_t *) data;

    assert(!guest_vspace->done_mapping);

    guest_vspace->iospaces = realloc(guest_vspace->iospaces, sizeof(guest_iospace_t) * (guest_vspace->num_iospaces + 1));
    assert(guest_vspace->iospaces);
    guest_vspace->iospaces[guest_vspace->num_iospaces] = calloc(1, sizeof(guest_iospace_t));
    assert(guest_vspace->iospaces[guest_vspace->num_iospaces]);

    guest_iospace_t *guest_iospace = guest_vspace->iospaces[guest_vspace->num_iospaces];
    guest_iospace->iospace = iospace;
    int error = sel4utils_get_vspace(loader, &guest_iospace->iospace_vspace, &guest_iospace->iospace_vspace_data,
                                     guest_vspace->vspace_data.vka, seL4_CapNull, NULL, NULL);
    if (error) {
        ZF_LOGE("Failed to allocate vspace for new iospace");
        return error;
    }

    guest_vspace->num_iospaces++;
    return 0;
}
#endif

int vm_get_guest_vspace(vspace_t *loader, vspace_t *vmm, vspace_t *new_vspace, vka_t *vka, seL4_CPtr page_directory) {
    int error;
    guest_vspace_t *vspace = malloc(sizeof(*vspace));
    if (!vspace) {
        ZF_LOGE("Malloc failed");
        return -1;
    }
#if defined(CONFIG_ARM_SMMU) || defined(CONFIG_IOMMU)
    vspace->done_mapping = 0;
    vspace->num_iospaces = 0;
    vspace->iospaces = malloc(0);
    assert(vspace->iospaces);
#endif
    vspace->vmm_vspace = *vmm;
    error = sel4utils_get_vspace(loader, &vspace->translation_vspace, &vspace->translation_vspace_data, vka, page_directory,
                                 NULL, NULL);
    if (error) {
        ZF_LOGE("Failed to create translation vspace");
        return error;
    }
    error = sel4utils_get_vspace_with_map(loader, new_vspace, &vspace->vspace_data, vka, page_directory, NULL, NULL,
                                          guest_vspace_map);
    if (error) {
        ZF_LOGE("Failed to create guest vspace");
        return error;
    }

    new_vspace->unmap_pages = guest_vspace_unmap;

    return 0;
}
