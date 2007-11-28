#include <stdio.h>
#include <assert.h>
#include "mempool.h"

void toku_mempool_init(struct mempool *mp, void *base, int size) {
    // printf("mempool_init %p %p %d\n", mp, base, size);
    assert(base != 0 && size >= 0);
    mp->base = base;
    mp->size = size;
    mp->free_offset = 0;
    mp->frag_size = 0;
    mp->compress_func = 0;
    mp->compress_arg = 0;
}

void toku_mempool_fini(struct mempool *mp __attribute__((unused))) {
    // printf("mempool_fini %p %p %d %d\n", mp, mp->base, mp->size, mp->frag_size);
}

void toku_mempool_set_compress_func(struct mempool *mp, mempool_compress_func compress_func, void *compress_arg) {
    mp->compress_func = compress_func;
    mp->compress_arg = compress_arg;
}

void toku_mempool_call_compress_func(struct mempool *mp) {
    mp->compress_func(mp, mp->compress_arg);
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

void *toku_mempool_malloc(struct mempool *mp, int size, int alignment) {
    assert(mp->free_offset <= mp->size);
    void *vp;
    int offset = (mp->free_offset + (alignment-1)) & ~(alignment-1);
    if (offset + size > mp->size) {
        vp = 0;
    } else {
        vp = mp->base + offset;
        mp->free_offset = offset + size;
    }
    assert(mp->free_offset <= mp->size);
    assert(((long)vp & (alignment-1)) == 0);
    assert(vp == 0 || (mp->base <= vp && vp + size <= mp->base + mp->size)); 
    return vp;
}

void toku_mempool_mfree(struct mempool *mp, void *vp, int size) {
    assert(size >= 0 && mp->base <= vp && vp + size <= mp->base + mp->size); 
    mp->frag_size += size;
    assert(mp->frag_size <= mp->size);
}
