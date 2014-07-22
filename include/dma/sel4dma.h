/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/**
 * OS specific (seL4) DMA memory manager initialisation interface.
 * This library requires an operational malloc/free.
 */

/* Author: alex.kroh@nicta.com.au */

#ifndef _DMA_SEL4DMA_H_
#define _DMA_SEL4DMA_H_

#include <dma/dma.h>

/* Hack for use without seL4 */
#ifndef __LIBSEL4_SEL4_H
#define seL4_CPtr uint32_t
#endif

/**
 * Describes a chunk of memory for DMA use.
 */
struct dma_mem_descriptor {
    /** The virtual address of the memory.
        It is the applications responsibility to perform the mapping operation. */
    uintptr_t vaddr;
    /// The physical address of the memory.
    uintptr_t paddr;
    /** Boolian representation of the cacheability of the frames in this 
        descriptor. */
    int       cached;
    /// The size of each frame (2^frame_size_bits bytes)
    int       size_bits;
    /// The capability to this frame 
    seL4_CPtr cap;
    /// This field is unused and may be used by the application.
    void*     cookie;
};

/**
 * A callback for providing DMA memory to the allocator.
 * @param[in]  min_size the minimum size for the allocation 
 * @param[in]  cached   0 if the requested memory should not be cached.
 * @param[out] dma_desc On return, this structure should be filled with a 
 *                      description of the memory that has been provided.
 * @return              0 on success
 */
typedef int (*dma_morecore_fn)(size_t min_size, int cached, 
                               struct dma_mem_descriptor* dma_desc);

/**
 * Initialises a new DMA allocator.
 * @param[in] morecore A function to use when the allocator requires more DMA
 *                     memory. NULL if the allocator should not request more
 *                     memory.
 * @return             A reference to a new DMA allocator instance.
 */
struct dma_allocator* dma_allocator_init(dma_morecore_fn morecore);

/**
 * Explicitly provides memory to the DMA allocator.
 * @param[in] dma_desc A description of the memory provided.
 * @return             0 on success
 */
int dma_provide_mem(struct dma_allocator* allocator,
                    struct dma_mem_descriptor dma_desc);

/**
 * If possible, reclaim some memory from the DMA allocator.
 * @param[in]  allocator A handle to the allocator to reclaim memory from
 * @param[out] dma_desc  If the call is successful, this structure will
 *                       be filled with a description of the memeory that
 *                       has been released from the allocator.
 * @return               0 on success, non-zero indicates that a suitable
 *                       reclaim candidate could not be found.
 */
int dma_reclaim_mem(struct dma_allocator* allocator,
                    struct dma_mem_descriptor* dma_desc);

#endif /* _DMA_SEL4DMA_H_ */
