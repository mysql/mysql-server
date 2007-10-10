#include <stdio.h>
#include <assert.h>
#include "mempool.h"

void mempool_init(struct mempool *mp, void *base, int size) {
    // printf("mempool_init %p %p %d\n", mp, base, size);
    assert(base != 0 && size > 0);
    mp->base = base;
    mp->size = size;
    mp->free_offset = 0;
    mp->frag_size = 0;
}

void mempool_fini(struct mempool *mp __attribute__((unused))) {
    // printf("mempool_fini %p %p %d %d\n", mp, mp->base, mp->size, mp->frag_size);
}

void mempool_get_base_size(struct mempool *mp, void **base_ptr, int *size_ptr) {
    *base_ptr = mp->base;
    *size_ptr = mp->size;
}

int mempool_get_frag_size(struct mempool *mp) {
    return mp->frag_size;
}

void *mempool_malloc(struct mempool *mp, int size, int alignment) {
    assert(mp->free_offset < mp->size);
    void *vp;
    int offset = (mp->free_offset + (alignment-1)) & ~(alignment-1);
    if (offset + size > mp->size) {
        vp = 0;
    } else {
        vp = mp->base + offset;
        mp->free_offset = offset + size;
    }
    assert(mp->free_offset < mp->size);
    assert(((long)vp & (alignment-1)) == 0);
    assert(vp == 0 || (mp->base <= vp && vp + size  < mp->base + mp->size)); 
    return vp;
}

void mempool_mfree(struct mempool *mp, void *vp, int size) {
    assert(size > 0 && mp->base <= vp && vp + size  < mp->base + mp->size); 
    mp->frag_size += size;
    assert(mp->frag_size <= mp->size);
}
