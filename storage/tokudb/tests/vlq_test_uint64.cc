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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <tokudb_vlq.h>

namespace tokudb {
    template size_t vlq_encode_ui(uint32_t n, void *p, size_t s);
    template size_t vlq_decode_ui(uint32_t *np, void *p, size_t s);
    template size_t vlq_encode_ui(uint64_t n, void *p, size_t s);
    template size_t vlq_decode_ui(uint64_t *np, void *p, size_t s);
};

// test a slice of the number space where the slice is described by
// a start number and a stride through the space.
static void test_vlq_uint64(uint64_t start, uint64_t stride) {
    printf("%u\n", 0);
    for (uint64_t v = 0 + start; v < (1<<7); v += stride) {
        unsigned char b[10];
        size_t out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
        assert(out_s == 1);
        uint64_t n;
        size_t in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
        assert(in_s == 1 && n == v);
    }

    printf("%u\n", 1<<7);
    for (uint64_t v = (1<<7) + start; v < (1<<14); v += stride) {
        unsigned char b[10];
        size_t out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
        assert(out_s == 2);
        uint64_t n;
        size_t in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
        assert(in_s == 2 && n == v);
    }

    printf("%u\n", 1<<14);
    for (uint64_t v = (1<<14) + start; v < (1<<21); v += stride) {
        unsigned char b[10];
        size_t out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
        assert(out_s == 3);
        uint64_t n;
        size_t in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
        assert(in_s == 3 && n == v);
    }

    printf("%u\n", 1<<21);
    for (uint64_t v = (1<<21) + start; v < (1<<28); v += stride) {
        unsigned char b[10];
        size_t out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
        assert(out_s == 4);
        uint64_t n;
        size_t in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
        assert(in_s == 4 && n == v);
    }

    printf("%u\n", 1<<28);
#if USE_OPENMP
#pragma omp parallel num_threads(4)
#pragma omp for
#endif
    for (uint64_t v = (1<<28) + start; v < (1ULL<<35); v += stride) {
        unsigned char b[10];
        size_t out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
        assert(out_s == 5);
        uint64_t n;
        size_t in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
        assert(in_s == 5 && n == v);
    }
}

int main(int argc, char *argv[]) {
    uint64_t start = 0, stride = 1;
    if (argc == 3) {
        start = atoll(argv[1]);
        stride = atoll(argv[2]);
    }
    test_vlq_uint64(start, stride);
    return 0;
}
