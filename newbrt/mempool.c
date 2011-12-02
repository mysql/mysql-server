/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: mempool.c 19902 2010-05-06 20:41:32Z bkuszmaul $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

/* Contract: 
 * Caller allocates mempool struct as convenient for caller, but memory used for data storage 
 * must be dynamically allocated via toku_malloc().
 * Caller dynamically allocates memory for mempool and initializes mempool by calling toku_mempool_init().
 * Once a buffer is assigned to a mempool (via toku_mempool_init()), the mempool owns it and 
 * is responsible for destroying it when the mempool is destroyed.
 * Caller destroys mempool by calling toku_mempool_destroy().
 *
 * Note, toku_mempool_init() does not allocate the memory because sometimes the caller will already have 
 * the memory allocated and will assign the pre-allocated memory to the mempool.
 */

/* This is a constructor to be used when the memory for the mempool struct has been
 * allocated by the caller, but no memory has yet been allocatd for the data.
 */
void toku_mempool_zero(struct mempool *mp) {
    // printf("mempool_zero %p\n", mp);
    memset(mp, 0, sizeof(*mp));
}

/* Copy constructor.  Any time a new mempool is needed, allocate 1/4 more space
 * than is currently needed.
 */
void toku_mempool_copy_construct(struct mempool *mp, const void * const data_source, const size_t data_size) {
    // printf("mempool_copy %p %p %lu\n", mp, data_source, data_size);
    if (data_size) {
	invariant(data_source);
	toku_mempool_construct(mp, data_size);
	memcpy(mp->base, data_source, data_size);
	mp->free_offset = data_size;                     // address of first available memory for new data
    }
    else {
	toku_mempool_zero(mp);
	//	fprintf(stderr, "Empty mempool created (copy constructor)\n");
    }
}

// TODO 4050 this is dirty, try to replace all uses of this
void toku_mempool_init(struct mempool *mp, void *base, size_t size) {
    // printf("mempool_init %p %p %lu\n", mp, base, size);
    invariant(base != 0);
    invariant(size < (1U<<31)); // used to be assert(size >= 0), but changed to size_t so now let's make sure it's not more than 2GB...
    mp->base = base;
    mp->size = size;
    mp->free_offset = 0;             // address of first available memory
    mp->frag_size = 0;               // byte count of wasted space (formerly used, no longer used or available)
}

/* allocate memory and construct mempool
 */
void toku_mempool_construct(struct mempool *mp, size_t data_size) {
    if (data_size) {
	size_t mpsize = data_size + (data_size/4);     // allow 1/4 room for expansion (would be wasted if read-only)
	mp->base = toku_xmalloc(mpsize);               // allocate buffer for mempool
	mp->size = mpsize;
	mp->free_offset = 0;                     // address of first available memory for new data
	mp->frag_size = 0;                       // all allocated space is now in use    
    }
    else {
	toku_mempool_zero(mp);
	//	fprintf(stderr, "Empty mempool created (base constructor)\n");
    }
}


void toku_mempool_destroy(struct mempool *mp) {
    // printf("mempool_destroy %p %p %lu %lu\n", mp, mp->base, mp->size, mp->frag_size);
    if (mp->base) 
	toku_free(mp->base);
    toku_mempool_zero(mp);
}

void *toku_mempool_get_base(struct mempool *mp) {
    return mp->base;
}

size_t toku_mempool_get_size(struct mempool *mp) {
    return mp->size;
}

size_t toku_mempool_get_frag_size(struct mempool *mp) {
    return mp->frag_size;
}

size_t toku_mempool_get_used_space(struct mempool *mp) {
    return mp->free_offset - mp->frag_size;
}

size_t toku_mempool_get_free_space(struct mempool *mp) {
    return mp->size - mp->free_offset;
}

size_t toku_mempool_get_allocated_space(struct mempool *mp) {
    return mp->free_offset;
}

void *toku_mempool_malloc(struct mempool *mp, size_t size, int alignment) {
    invariant(size < (1U<<31));
    invariant(mp->size < (1U<<31));
    invariant(mp->free_offset < (1U<<31));
    assert(mp->free_offset <= mp->size);
    void *vp;
    size_t offset = (mp->free_offset + (alignment-1)) & ~(alignment-1);
    //printf("mempool_malloc size=%ld base=%p free_offset=%ld mp->size=%ld offset=%ld\n", size, mp->base, mp->free_offset, mp->size, offset);
    if (offset + size > mp->size) {
        vp = 0;
    } else {
        vp = (char *)mp->base + offset;
        mp->free_offset = offset + size;
    }
    assert(mp->free_offset <= mp->size);
    assert(((long)vp & (alignment-1)) == 0);
    assert(vp == 0 || toku_mempool_inrange(mp, vp, size));
    //printf("mempool returning %p\n", vp);
    return vp;
}

// if vp is null then we are freeing something, but not specifying what.  The data won't be freed until compression is done.
void toku_mempool_mfree(struct mempool *mp, void *vp, size_t size) {
    if (vp) assert(toku_mempool_inrange(mp, vp, size));
    mp->frag_size += size;
    assert(mp->frag_size <= mp->size);
}


/* get memory footprint */
size_t toku_mempool_footprint(struct mempool *mp) {
    void * base = mp->base;
    size_t touched = mp->free_offset;
    size_t rval = toku_memory_footprint(base, touched);
    return rval;
}
