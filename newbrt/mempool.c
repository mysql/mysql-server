/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "mempool.h"
#include "toku_assert.h"
#include <stdio.h>

void toku_mempool_init(struct mempool *mp, void *base, size_t size) {
    // printf("mempool_init %p %p %d\n", mp, base, size);
    assert(base != 0 && size<(1U<<31)); // used to be assert(size >= 0), but changed to size_t so now let's make sure it's not more than 2GB...
    mp->base = base;
    mp->size = size;
    mp->free_offset = 0;
    mp->frag_size = 0;
}

void toku_mempool_fini(struct mempool *mp __attribute__((unused))) {
    // printf("mempool_fini %p %p %d %d\n", mp, mp->base, mp->size, mp->frag_size);
}

void *toku_mempool_get_base(struct mempool *mp) {
    return mp->base;
}

int toku_mempool_get_size(struct mempool *mp) {
    return mp->size;
}

int toku_mempool_get_frag_size(struct mempool *mp) {
    return mp->frag_size;
}

void *toku_mempool_malloc(struct mempool *mp, size_t size, int alignment) {
    assert(mp->free_offset <= mp->size);
    void *vp;
    size_t offset = (mp->free_offset + (alignment-1)) & ~(alignment-1);
    //printf("mempool_malloc size=%ld base=%p free_offset=%ld mp->size=%ld offset=%ld\n", size, mp->base, mp->free_offset, mp->size, offset);
    if (offset + size > mp->size) {
        vp = 0;
    } else {
        vp = mp->base + offset;
        mp->free_offset = offset + size;
    }
    assert(mp->free_offset <= mp->size);
    assert(((long)vp & (alignment-1)) == 0);
    assert(vp == 0 || (mp->base <= vp && vp + size <= mp->base + mp->size)); 
    //printf("mempool returning %p\n", vp);
    return vp;
}

// if vp is null then we are freeing something, but not specifying what.  The data won't be freed until compression is done.
void toku_mempool_mfree(struct mempool *mp, void *vp, int size) {
    assert(size >= 0);
    if (vp) assert(toku_mempool_inrange(mp, vp, size));
    mp->frag_size += size;
    assert(mp->frag_size <= mp->size);
}

unsigned long toku_mempool_memory_size(struct mempool *mp) {
    return mp->size; 
}
