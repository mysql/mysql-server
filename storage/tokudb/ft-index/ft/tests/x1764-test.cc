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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."



#include "test.h"
static void
test0 (void) {
    uint32_t c = x1764_memory("", 0);
    assert(c==~(0U));
    struct x1764 cs;
    x1764_init(&cs);
    x1764_add(&cs, "", 0);
    c = x1764_finish(&cs);
    assert(c==~(0U));
}

static void
test1 (void) {
    uint64_t v=0x123456789abcdef0ULL;
    uint32_t c;
    int i;
    for (i=0; i<=8; i++) {
	uint64_t expect64 = (i==8) ? v : v&((1LL<<(8*i))-1);
	uint32_t expect = expect64 ^ (expect64>>32);
	c = x1764_memory(&v, i);
	//printf("i=%d c=%08x expect=%08x\n", i, c, expect);
	assert(c==~expect);
    }
}

// Compute checksums incrementally, using various strides
static void
test2 (void) {
    enum { N=200 };
    char v[N];
    int i;
    for (i=0; i<N; i++) v[i]=(char)random();
    for (i=0; i<N; i++) {
	int j;
	for (j=i; j<=N; j++) {
	    // checksum from i (inclusive to j (exclusive)
	    uint32_t c = x1764_memory(&v[i], j-i);
	    // Now compute the checksum incrementally with various strides.
	    int stride;
	    for (stride=1; stride<=j-i; stride++) {
		int k;
		struct x1764 s;
		x1764_init(&s);
		for (k=i; k+stride<=j; k+=stride) {
		    x1764_add(&s, &v[k], stride);
		}
		x1764_add(&s, &v[k], j-k);
		uint32_t c2 = x1764_finish(&s);
		assert(c2==c);
	    }
	    // Now use some random strides.
	    {
		int k=i;
		struct x1764 s;
		x1764_init(&s);
		while (1) {
		    stride=random()%16;
		    if (k+stride>j) break;
		    x1764_add(&s, &v[k], stride);
		    k+=stride;
		}
		x1764_add(&s, &v[k], j-k);
		uint32_t c2 = x1764_finish(&s);
		assert(c2==c);
	    }
	}
    }
}

static void
test3 (void)
// Compare the simple version to the highly optimized verison.
{
    const int datalen = 1000;
    char data[datalen];
    for (int i=0; i<datalen; i++) data[i]=random();
    for (int off=0; off<32; off++) {
	if (verbose) {printf("."); fflush(stdout);}
	for (int len=0; len+off<datalen; len++) {
	    uint32_t reference_sum = x1764_memory_simple(data+off, len);
	    uint32_t fast_sum      = x1764_memory       (data+off, len);
	    assert(reference_sum==fast_sum);
	}
    }
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    if (verbose) printf("0\n");
    test0();
    if (verbose) printf("1\n");
    test1();
    if (verbose) printf("2\n");
    test2();
    if (verbose) printf("3\n");
    test3();
    return 0;
}
