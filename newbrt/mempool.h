#ifndef _TOKU_MEMPOOL_H
#define _TOKU_MEMPOOL_H
#ident "$Id: mempool.h 19902 2010-05-06 20:41:32Z bkuszmaul $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* a memory pool is a contiguous region of memory that supports single
   allocations from the pool.  these allocated regions are never recycled.
   when the memory pool no longer has free space, the allocated chunks
   must be relocated by the application to a new memory pool. */

#include <sys/types.h>

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

struct mempool;

  // TODO 4050 Hide mempool struct internals from callers

struct mempool {
    void *base;           /* the base address of the memory */
    size_t free_offset;      /* the offset of the memory pool free space */
    size_t size;             /* the size of the memory */
    size_t frag_size;        /* the size of the fragmented memory */
};

/* This is a constructor to be used when the memory for the mempool struct has been
 * allocated by the caller, but no memory has yet been allocatd for the data.
 */
void toku_mempool_zero(struct mempool *mp);

/* Copy constructor.  Fill in empty mempool struct with new values, allocating 
 * a new buffer and filling the buffer with data from from data_source.
 * Any time a new mempool is needed, allocate 1/4 more space
 * than is currently needed.  
 */
void toku_mempool_copy_construct(struct mempool *mp, const void * const data_source, const size_t data_size);

/* initialize the memory pool with the base address and size of a
   contiguous chunk of memory */
void toku_mempool_init(struct mempool *mp, void *base, size_t size);

/* allocate memory and construct mempool
 */
void toku_mempool_construct(struct mempool *mp, size_t data_size);

/* destroy the memory pool */
void toku_mempool_destroy(struct mempool *mp);

/* get the base address of the memory pool */
void *toku_mempool_get_base(struct mempool *mp);

/* get the size of the memory pool */
size_t toku_mempool_get_size(struct mempool *mp);

/* get the amount of fragmented (wasted) space in the memory pool */
size_t toku_mempool_get_frag_size(struct mempool *mp);

/* get the amount of space that is holding useful data */
size_t toku_mempool_get_used_space(struct mempool *mp);

/* get the amount of space that is available for new data */
size_t toku_mempool_get_free_space(struct mempool *mp);


/* allocate a chunk of memory from the memory pool suitably aligned */
void *toku_mempool_malloc(struct mempool *mp, size_t size, int alignment);

/* free a previously allocated chunk of memory.  the free only updates
   a count of the amount of free space in the memory pool.  the memory
   pool does not keep track of the locations of the free chunks */
void toku_mempool_mfree(struct mempool *mp, void *vp, size_t size);

/* verify that a memory range is contained within a mempool */
static inline int toku_mempool_inrange(struct mempool *mp, void *vp, size_t size) {
    return (mp->base <= vp) && ((char *)vp + size <= (char *)mp->base + mp->size);
}

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif


#endif
