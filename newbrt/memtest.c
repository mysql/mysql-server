/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "memory.h"
#include "mempool.h"

void test_mempool_limits(int size) {
    void *base = malloc(size);
    struct mempool mempool;
    mempool_init(&mempool, base, size);

    int i;
    for (i=0;; i++) {
        void *vp = mempool_malloc(&mempool, 1, 1);
        if (vp == 0) 
            break;
    }
    assert(i == size);

    mempool_fini(&mempool);
    free(base);
}

void test_mempool_malloc_mfree(int size) {
    void *base = malloc(size);
    struct mempool mempool;
    mempool_init(&mempool, base, size);

    void *vp[size];
    int i;
    for (i=0;; i++) {
        vp[i] = mempool_malloc(&mempool, 1, 1);
        if (vp[i] == 0) 
            break;
    }
    assert(i == size);

    for (i=0; i<size; i++) 
        mempool_mfree(&mempool, vp[i], 1);
    assert(mempool_get_frag_size(&mempool) == size);

    mempool_fini(&mempool);
    free(base);
}

int main() {
    test_mempool_limits(0);
    test_mempool_limits(256);
    test_mempool_malloc_mfree(0);
    test_mempool_malloc_mfree(256);
    return 0;
}
