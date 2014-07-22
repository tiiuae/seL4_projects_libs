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
 * OS agnostic interface for DMA memory operations
 */

/* Author: alex.kroh@nicta.com.au */

#ifndef _DMA_DMA_H_
#define _DMA_DMA_H_

#include <stdint.h>
#include <stddef.h>

struct dma_allocator;
typedef struct dma_mem* dma_mem_t;


typedef void* vaddr_t;
typedef uintptr_t paddr_t;

/**
 * DMA access flags.
 * DMA access patterns are important to the DMA memory manager because cache
 * maintenance operations may affect (corrupt) adjacent buffers. This is due
 * to cach line size limits and software flushing granularity (perhaps the OS
 * interface limits us to perform maintenance operations on a per page basis).
 * 
 * These flags will generally lead to multiple memory pools where each request
 * will be allocated from the appropriate pool.
 */
enum dma_flags {
    /// Host always reads, never writes (invalidate only)
    DMAF_HR,
    /// Host always writes, never reads (clean only)
    DMAF_HW,
    /// Host performs both read and write (clean/invalidate)
    DMAF_HRW,
    /// Explicitly request uncached DMA memory
    DMAF_COHERENT
};

/**
 * Retrieve the virtual address of allocated DMA memory.
 * @param[in] dma_mem       A handle to DMA memory.
 * @return                  The virtual address of the DMA memory in question.
 */
vaddr_t dma_vaddr(dma_mem_t dma_mem);

/**
 * Retrieve the physical address of allocated DMA memory.
 * @param[in] dma_mem       A handle to DMA memory.
 * @return                  The physical address of the DMA memory in question.
 */
paddr_t dma_paddr(dma_mem_t dma_mem);



/**
 * Flush DMA memory out to RAM by virtual address
 * @param[in] dma_mem A handle to allocated DMA memory.
 * @param[in] vstart  The staring virtual address of the flush.
 * @param[in] vend    One greater than the last virtual address to be
 *                    flushed.
 */
void dma_clean(dma_mem_t dma_mem, vaddr_t vstart, vaddr_t vend);

/**
 * Invalidate DMA memory from cache by virtual address
 * @param[in] dma_mem A handle to allocated DMA memory.
 * @param[in] vstart  The staring virtual address of the invalidation.
 * @param[in] vend    One greater than the last virtual address to be
 *                    invalidated.
 */
void dma_invalidate(dma_mem_t dma_mem, vaddr_t vstart, vaddr_t vend);

/**
 * Flush DMA memory out to RAM and invalidate the caches by virtual address.
 * @param[in] dma_mem A handle to allocated DMA memory.
 * @param[in] vstart  The staring virtual address of the clean/invalidate.
 * @param[in] vend    One greater than the last virtual address to be
 *                          cleaned/invalidated.
 */
void dma_cleaninvalidate(dma_mem_t dma_mem, vaddr_t vstart, vaddr_t vend);

/**
 * Allocate DMA memory.
 * @param[in]  allocator The DMA allocator instance to use for the allocation.
 * @param[in]  size      The allocation size.
 * @param[in]  align     The minimum alignment (in bytes) of the allocated 
 *                       region.
 * @param[in]  flags     The allocation properties of the request.
 * @param[out] dma_mem   If the call is successful and dma_mem is not NULL, 
 *                       dma_mem will contain a handle to the allocated memory.
 * @return               The virtual address of the allocated DMA memory, NULL
 *                       on failure.
 */
vaddr_t dma_alloc(struct dma_allocator* allocator, size_t size, int align,
                  enum dma_flags flags, dma_mem_t* dma_mem);

/**
 * Free DMA memory by virtual address.
 * @param[in] dma_mem   A handle to the DMA memory to free.
 */
void dma_free(dma_mem_t dma_mem);


/***************
 * The use of the following functions is not recommended due to performance
 * limitations, however, they may come in handy for debugging or rapid
 * prototyping.
 ***************/


/**
 * Retrieve the DMA memory handle from a given physical address. 
 * The performance of this operation is likely to be poor, but it may be useful
 * for rapid prototyping or debugging.
 * @param[in] allocator The allocator managing the memory.
 * @param[in] paddr     The physical address of the memory in question
 * @return              The DMA memory handle associated with the
 *                      provided physical address. NULL if the physical
 *                      address is not managed by the provided allocator.
 */
dma_mem_t dma_plookup(struct dma_allocator* allocator, paddr_t paddr);

/**
 * Retrieve the DMA memory handle from a given virtual address.
 * The performance of this operation is likely to be poor, but it may be useful
 * for rapid prototyping or debugging.
 * @param[in] allocator The allocator managing the memory.
 * @param[in] vaddr     The virtual address of the memory in question
 * @return              The DMA memory handle associated with the
 *                      provided virtual address. NULL if the physical
 *                      address is not managed by the provided allocator.
 */
dma_mem_t dma_vlookup(struct dma_allocator* allocator, vaddr_t vaddr);


#endif /* _DMA_DMA_H_ */
