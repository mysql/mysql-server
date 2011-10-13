/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2009-2010 Tokutek Inc.  All rights reserved."
#include "../block_allocator.h"
#include <memory.h>
#include <assert.h>
// Test the merger.

int verbose = 0;

static void
print_array (u_int64_t n, const struct block_allocator_blockpair a[/*n*/]) {
    printf("{");
    for (u_int64_t i=0; i<n; i++) printf(" %016lx", (long)a[i].offset);
    printf("}\n");
}

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
    if (verbose>1) { printf("a:"); print_array(an, a); }
    if (verbose>1) { printf("b:"); print_array(bn, b); }
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
    if (verbose) printf("qsort\n");
    qsort(q, an+bn, sizeof(*q), compare_blockpairs);
    if (verbose>1) { printf("q:"); print_array(an+bn, q); }
    if (verbose) printf("merge\n");
    block_allocator_merge_blockpairs_into(an, m, bn, b);
    if (verbose) printf("compare\n");
    if (verbose>1) { printf("m:"); print_array(an+bn, m); }
    for (u_int64_t i=0; i<an+bn; i++) {
	assert(q[i].offset == m[i].offset);
    }
 malloc_failed:
    toku_free(q);
    toku_free(m);
}

static u_int64_t
compute_a (u_int64_t i, int mode) {
    if (mode==0) return (((u_int64_t)random()) << 32) + i;
    if (mode==1) return 2*i;
    if (mode==2) return i;
    if (mode==3) return (1LL<<50) + i;
    abort();
}
static u_int64_t
compute_b (u_int64_t i, int mode) {
    if (mode==0) return (((u_int64_t)random()) << 32) + i;
    if (mode==1) return 2*i+1;
    if (mode==2) return (1LL<<50) + i;
    if (mode==3) return i;
    abort();
}
    

static void
test_merge_n_m (u_int64_t n, u_int64_t m, int mode)
{
    struct block_allocator_blockpair *MALLOC_N(n, na);
    struct block_allocator_blockpair *MALLOC_N(m, ma);
    if (na==0 || ma==0) {
	fprintf(stderr, "malloc failed, continuing\n");
	goto malloc_failed;
    }
    if (verbose) printf("Filling a[%ld]\n", n);
    for (u_int64_t i=0; i<n; i++) {
	na[i].offset = compute_a(i, mode);
    }
    if (verbose) printf("Filling b[%ld]\n", m);
    for (u_int64_t i=0; i<m; i++) {
	if (verbose && i % (1+m/10) == 0) { printf("."); fflush(stdout); }
	ma[i].offset = compute_b(i, mode);
    }
    qsort(na, n, sizeof(*na), compare_blockpairs);
    qsort(ma, m, sizeof(*ma), compare_blockpairs);
    if (verbose) fprintf(stderr, "\ntest_merge\n");
    test_merge(n, na, m, ma);
 malloc_failed:
    toku_free(na);
    toku_free(ma);
}

static void
test_big_merge (void) {
    u_int64_t G = 1024LL * 1024LL * 1024LL;
    if (toku_os_get_phys_memory_size() < 40 * G) {
	fprintf(stderr, "Skipping big merge because there is only %4.1fGiB physical memory\n", toku_os_get_phys_memory_size()/(1024.0*1024.0*1024.0));
    } else {
	u_int64_t twoG = 2*G;

	u_int64_t an = twoG;
	u_int64_t bn = 1;
	struct block_allocator_blockpair *MALLOC_N(an+bn, a); 
        assert(a);
	struct block_allocator_blockpair *MALLOC_N(bn,    b);
        assert(b);
	for (u_int64_t i=0; i<an; i++) a[i].offset=i+1;
	b[0].offset = 0;
	block_allocator_merge_blockpairs_into(an, a, bn, b);
	for (u_int64_t i=0; i<an+bn; i++) assert(a[i].offset == i);
	toku_free(a);
	toku_free(b);
    }
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test_merge_n_m(4, 4, 0);
    test_merge_n_m(16, 16, 0);
    test_merge_n_m(0, 100, 0);
    test_merge_n_m(100, 0, 0);
    test_merge_n_m(1000000, 1000000, 0);
    // Cannot run this on my laptop, or even on pointy
#if 0
    u_int64_t too_big = 1024LL * 1024LL * 1024LL * 2;
    test_merge_n_m(too_big, too_big);
    test_merge_n_m(1, too_big, 0);
#endif
    test_big_merge();
    return 0;
}
