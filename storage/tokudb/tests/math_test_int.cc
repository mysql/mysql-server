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
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <tokudb_math.h>
using namespace tokudb;

static int64_t sign_extend(uint length_bits, int64_t n) {
    return n | ~((1ULL<<(length_bits-1))-1);
}

static void test_int_range(uint length_bits) {
    assert(int_high_endpoint(length_bits) == (int64_t)((1ULL<<(length_bits-1))-1));
    assert(int_low_endpoint(length_bits) == sign_extend(length_bits, 1ULL<<(length_bits-1)));
}

static void test_int8() {
    printf("%s\n", __FUNCTION__);
    test_int_range(8);
    int64_t max = (1LL << 7);
    for (int64_t x = -max; x <= max-1; x++) {
        for (int64_t y = -max; y <= max-1; y++) {
            bool over;
            int64_t n, m;
            n = int_add(x, y, 8, &over);
            m = x + y;
            if (m > max-1)
                assert(over);
            else if (m < -max)
                assert(over);
            else
                assert(!over && n == m);
            n = int_sub(x, y, 8, &over);
            m = x - y;
            if (m > max-1)
                assert(over);
            else if (m < -max)
                assert(over);
            else
                assert(!over && n == m);
        }
    }
}

static void test_int16() {
    printf("%s\n", __FUNCTION__);
    test_int_range(16);
    int64_t max = (1LL << 15);
    for (int64_t x = -max; x <= max-1; x++) {
        for (int64_t y = -max; y <= max-1; y++) {
            bool over;
            int64_t n, m;
            n = int_add(x, y, 16, &over);
            m = x + y;
            if (m > max-1)
                assert(over);
            else if (m < -max)
                assert(over);
            else
                assert(!over && n == m);
            n = int_sub(x, y, 16, &over);
            m = x - y;
            if (m > max-1)
                assert(over);
            else if (m < -max)
                assert(over);
            else
                assert(!over && n == m);
        }
    }
}

static void test_int24() {
    printf("%s\n", __FUNCTION__);
    test_int_range(24);
    int64_t s;
    bool over;

    s = int_add(1, (1ULL<<23)-1, 24, &over); assert(over);
    s = int_add((1ULL<<23)-1, 1, 24, &over); assert(over);
    s = int_sub(-1, (1ULL<<23), 24, &over); assert(!over && s == (1ULL<<23)-1);
    s = int_sub((1ULL<<23), 1, 24, &over); assert(over);

    s = int_add(0, 0, 24, &over); assert(!over && s == 0);
    s = int_sub(0, 0, 24, &over); assert(!over && s == 0);
    s = int_add(0, -1, 24, &over); assert(!over && s == -1);
    s = int_sub(0, 1, 24, &over); assert(!over && s == -1);
    s = int_add(0, (1ULL<<23), 24, &over); assert(!over && (s & ((1ULL<<24)-1)) == (1ULL<<23));
    s = int_sub(0, (1ULL<<23)-1, 24, &over); assert(!over && (s & ((1ULL<<24)-1)) == (1ULL<<23)+1);

    s = int_add(-1, 0, 24, &over); assert(!over && s == -1);
    s = int_add(-1, 1, 24, &over); assert(!over && s == 0);
    s = int_sub(-1, -1, 24, &over); assert(!over && s == 0);
    s = int_sub(-1, (1ULL<<23)-1, 24, &over); assert(!over && (s & ((1ULL<<24)-1)) == (1ULL<<23));
}

static void test_int32() {
    printf("%s\n", __FUNCTION__);
    test_int_range(32);
    int64_t s;
    bool over;

    s = int_add(1, (1ULL<<31)-1, 32, &over); assert(over);
    s = int_add((1ULL<<31)-1, 1, 32, &over); assert(over);
    s = int_sub(-1, (1ULL<<31), 32, &over); assert(s == (1ULL<<31)-1 && !over);
    s = int_sub((1ULL<<31), 1, 32, &over); assert(over);

    s = int_add(0, 0, 32, &over); assert(s == 0 && !over);
    s = int_sub(0, 0, 32, &over); assert(s == 0 && !over);
    s = int_add(0, -1, 32, &over); assert(s == -1 && !over);
    s = int_sub(0, 1, 32, &over); assert(s == -1 && !over);
    s = int_add(0, (1ULL<<31), 32, &over); assert((s & ((1ULL<<32)-1)) == (1ULL<<31) && !over);
    s = int_sub(0, (1ULL<<31)-1, 32, &over); assert((s & ((1ULL<<32)-1)) == (1ULL<<31)+1 && !over);

    s = int_add(-1, 0, 32, &over); assert(s == -1 && !over);
    s = int_add(-1, 1, 32, &over); assert(s == 0 && !over);
    s = int_sub(-1, -1, 32, &over); assert(s == 0 && !over);
    s = int_sub(-1, (1ULL<<31)-1, 32, &over); assert((s & ((1ULL<<32)-1)) == (1ULL<<31) && !over);
}

static void test_int64() {
    printf("%s\n", __FUNCTION__);
    test_int_range(64);
    int64_t s;
    bool over;

    s = int_add(1, (1ULL<<63)-1, 64, &over); assert(over);
    s = int_add((1ULL<<63)-1, 1, 64, &over); assert(over);
    s = int_sub(-1, (1ULL<<63), 64, &over); assert(s == (1ULL<<63)-1 && !over);
    s = int_sub((1ULL<<63), 1, 64, &over); assert(over);

    s = int_add(0, 0, 64, &over); assert(s == 0 && !over);
    s = int_sub(0, 0, 64, &over); assert(s == 0 && !over);
    s = int_add(0, -1, 64, &over); assert(s == -1 && !over);
    s = int_sub(0, 1, 64, &over); assert(s == -1 && !over);
    s = int_add(0, (1ULL<<63), 64, &over); assert(s == (int64_t)(1ULL<<63) && !over);
    s = int_sub(0, (1ULL<<63)-1, 64, &over); assert(s == (int64_t)((1ULL<<63)+1) && !over);

    s = int_add(-1, 0, 64, &over); assert(s == -1 && !over);
    s = int_add(-1, 1, 64, &over); assert(s == 0 && !over);
    s = int_sub(-1, -1, 64, &over); assert(s == 0 && !over);
    s = int_sub(-1, (1ULL<<63)-1, 64, &over); assert(s == (int64_t)(1ULL<<63) && !over);
}

static void test_int_sign(uint length_bits) {
    printf("%s %u\n", __FUNCTION__, length_bits);
    int64_t n;
    
    n = int_high_endpoint(length_bits);
    assert(int_sign_extend(n, length_bits) == n);
    n = (1ULL<<(length_bits-1));
    assert(int_sign_extend(n, length_bits) == -n);
}

static void test_int_sign() {
    test_int_sign(8);
    test_int_sign(16);
    test_int_sign(24);
    test_int_sign(32);
    test_int_sign(64);
}

int main() {
    test_int_sign();
    test_int8();
    test_int16();
    test_int24();
    test_int32();
    test_int64();
    return 0;
}

