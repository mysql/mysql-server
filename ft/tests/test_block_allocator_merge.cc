/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#include "../block_allocator.h"
#include <memory.h>
#include <assert.h>
// Test the merger.

int verbose = 0;

static void
print_array (uint64_t n, const struct block_allocator_blockpair a[/*n*/]) {
    printf("{");
    for (uint64_t i=0; i<n; i++) printf(" %016lx", (long)a[i].offset);
    printf("}\n");
}

static int
compare_blockpairs (const void *av, const void *bv) {
    const struct block_allocator_blockpair *CAST_FROM_VOIDP(a, av);
    const struct block_allocator_blockpair *CAST_FROM_VOIDP(b, bv);
    if (a->offset < b->offset) return -1;
    if (a->offset > b->offset) return +1;
    return 0;
}

static void
test_merge (uint64_t an, const struct block_allocator_blockpair a[/*an*/],
	    uint64_t bn, const struct block_allocator_blockpair b[/*bn*/]) {
    if (verbose>1) { printf("a:"); print_array(an, a); }
    if (verbose>1) { printf("b:"); print_array(bn, b); }
    struct block_allocator_blockpair *MALLOC_N(an+bn, q);
    struct block_allocator_blockpair *MALLOC_N(an+bn, m);
    if (q==0 || m==0) {
	fprintf(stderr, "malloc failed, continuing\n");
	goto malloc_failed;
    }
    for (uint64_t i=0; i<an; i++) {
	q[i] = m[i] = a[i];
    }
    for (uint64_t i=0; i<bn; i++) {
	q[an+i] = b[i];
    }
    if (verbose) printf("qsort\n");
    qsort(q, an+bn, sizeof(*q), compare_blockpairs);
    if (verbose>1) { printf("q:"); print_array(an+bn, q); }
    if (verbose) printf("merge\n");
    block_allocator_merge_blockpairs_into(an, m, bn, b);
    if (verbose) printf("compare\n");
    if (verbose>1) { printf("m:"); print_array(an+bn, m); }
    for (uint64_t i=0; i<an+bn; i++) {
	assert(q[i].offset == m[i].offset);
    }
 malloc_failed:
    toku_free(q);
    toku_free(m);
}

static uint64_t
compute_a (uint64_t i, int mode) {
    if (mode==0) return (((uint64_t)random()) << 32) + i;
    if (mode==1) return 2*i;
    if (mode==2) return i;
    if (mode==3) return (1LL<<50) + i;
    abort();
}
static uint64_t
compute_b (uint64_t i, int mode) {
    if (mode==0) return (((uint64_t)random()) << 32) + i;
    if (mode==1) return 2*i+1;
    if (mode==2) return (1LL<<50) + i;
    if (mode==3) return i;
    abort();
}
    

static void
test_merge_n_m (uint64_t n, uint64_t m, int mode)
{
    struct block_allocator_blockpair *MALLOC_N(n, na);
    struct block_allocator_blockpair *MALLOC_N(m, ma);
    if (na==0 || ma==0) {
	fprintf(stderr, "malloc failed, continuing\n");
	goto malloc_failed;
    }
    if (verbose) printf("Filling a[%" PRIu64 "]\n", n);
    for (uint64_t i=0; i<n; i++) {
	na[i].offset = compute_a(i, mode);
    }
    if (verbose) printf("Filling b[%" PRIu64 "]\n", m);
    for (uint64_t i=0; i<m; i++) {
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
    uint64_t G = 1024LL * 1024LL * 1024LL;
    if (toku_os_get_phys_memory_size() < 40 * G) {
	fprintf(stderr, "Skipping big merge because there is only %4.1fGiB physical memory\n", toku_os_get_phys_memory_size()/(1024.0*1024.0*1024.0));
    } else {
	uint64_t twoG = 2*G;

	uint64_t an = twoG;
	uint64_t bn = 1;
	struct block_allocator_blockpair *MALLOC_N(an+bn, a);
        struct block_allocator_blockpair *MALLOC_N(bn,    b);
        if (a == nullptr) {
            fprintf(stderr, "%s:%u malloc failed, continuing\n", __FUNCTION__, __LINE__);
            goto malloc_failed;
        }
        if (b == nullptr) {
            fprintf(stderr, "%s:%u malloc failed, continuing\n", __FUNCTION__, __LINE__);
            goto malloc_failed;
        }
        assert(a);
        assert(b);
        for (uint64_t i=0; i<an; i++) a[i].offset=i+1;
        b[0].offset = 0;
        block_allocator_merge_blockpairs_into(an, a, bn, b);
        for (uint64_t i=0; i<an+bn; i++) assert(a[i].offset == i);
    malloc_failed:
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
    uint64_t too_big = 1024LL * 1024LL * 1024LL * 2;
    test_merge_n_m(too_big, too_big);
    test_merge_n_m(1, too_big, 0);
#endif
    test_big_merge();
    return 0;
}
