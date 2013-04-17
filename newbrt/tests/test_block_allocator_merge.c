/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009-2010 Tokutek Inc.  All rights reserved."
#include "../block_allocator.h"
#include <memory.h>
#include <assert.h>
// Test the merger.

#if 0
static void
print_array (u_int64_t n, const struct block_allocator_blockpair a[/*n*/]) {
    printf("{");
    for (u_int64_t i=0; i<n; i++) printf(" %016lx", (long)a[i].offset);
    printf("}\n");
}
#endif

static int
compare_blockpairs (const void *av, const void *bv) {
    const struct block_allocator_blockpair *a = av;
    const struct block_allocator_blockpair *b = bv;
    if (a->offset < b->offset) return -1;
    if (a->offset > b->offset) return +1;
    return 0;
}

static void
test_merge (u_int64_t an, const struct block_allocator_blockpair a[/*an*/],
	    u_int64_t bn, const struct block_allocator_blockpair b[/*bn*/]) {
    //printf("a:"); print_array(an, a);
    //printf("b:"); print_array(bn, b);
    struct block_allocator_blockpair *MALLOC_N(an+bn, q);
    struct block_allocator_blockpair *MALLOC_N(an+bn, m);
    if (q==0 || m==0) {
	fprintf(stderr, "malloc failed, continuing\n");
	goto malloc_failed;
    }
    for (u_int64_t i=0; i<an; i++) {
	q[i] = m[i] = a[i];
    }
    for (u_int64_t i=0; i<bn; i++) {
	q[an+i] = b[i];
    }
    qsort(q, an+bn, sizeof(*q), compare_blockpairs);
    //printf("q:"); print_array(an+bn, q);
    block_allocator_merge_blockpairs_into(an, m, bn, b);
    //printf("m:"); print_array(an+bn, m);
    for (u_int64_t i=0; i<an+bn; i++) {
	assert(q[i].offset == m[i].offset);
    }
 malloc_failed:
    toku_free(q);
    toku_free(m);
}

static void
test_merge_n_m (u_int64_t n, u_int64_t m)
{
    struct block_allocator_blockpair *MALLOC_N(n, na);
    struct block_allocator_blockpair *MALLOC_N(m, ma);
    if (na==0 || ma==0) {
	fprintf(stderr, "malloc failed, continuing\n");
	goto malloc_failed;
    }
    for (u_int64_t i=0; i<n; i++) {
	na[i].offset = (((u_int64_t)random())<<32) + random();
    }
    for (u_int64_t i=0; i<m; i++) {
	ma[i].offset = (((u_int64_t)random())<<32) + random();
    }
    qsort(na, n, sizeof(*na), compare_blockpairs);
    qsort(ma, m, sizeof(*ma), compare_blockpairs);
    test_merge(n, na, m, ma);
 malloc_failed:
    toku_free(na);
    toku_free(ma);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test_merge_n_m(4, 4);
    test_merge_n_m(16, 16);
    test_merge_n_m(0, 100);
    test_merge_n_m(100, 0);
    // Cannot run this on my laptop
    u_int64_t too_big = 1024LL * 1024LL * 1024LL * 2;
    test_merge_n_m(too_big, too_big);
    return 0;
}
