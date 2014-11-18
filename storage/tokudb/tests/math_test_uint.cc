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

static void test_uint_range(uint length_bits) {
    assert(uint_low_endpoint(length_bits) == 0);
    if (length_bits == 64)
        assert(uint_high_endpoint(length_bits) == ~0ULL);
    else
        assert(uint_high_endpoint(length_bits) == (1ULL<<length_bits)-1);
}

static void test_uint8() {
    printf("%s\n", __FUNCTION__);
    test_uint_range(8);
    bool over;
    uint8_t n;
    uint64_t m;
    for (uint64_t x = 0; x <= (1ULL<<8)-1; x++) {
        for (uint64_t y = 0; y <= (1ULL<<8)-1; y++) {
            n = uint_add(x, y, 8, &over);
            m = x + y;
            if (m > (1ULL<<8)-1)
                assert(over);
            else 
                assert(!over && n == (m % 256));
            n = uint_sub(x, y, 8, &over);
            m = x - y;
            if (m > x)
                assert(over);
            else
                assert(!over && n == (m % 256));
        }
    }
}

static void test_uint16() {
    printf("%s\n", __FUNCTION__);
    test_uint_range(16);
    bool over;
    uint16_t n;
    uint64_t m;
    for (uint64_t x = 0; x <= (1ULL<<16)-1; x++) {
        for (uint64_t y = 0; y <= (1ULL<<16)-1; y++) {
            n = uint_add(x, y, 16, &over);
            m = x + y;
            if (m > (1ULL<<16)-1)
                assert(over);
            else
                assert(!over && n == (m % (1ULL<<16)));
            n = uint_sub(x, y, 16, &over);
            m = x - y;
            if (m > x)
                assert(over);
            else
                assert(!over && n == (m % (1ULL<<16)));
        }
    }
}

static void test_uint24() {
    printf("%s\n", __FUNCTION__);
    test_uint_range(24);
    bool over;
    uint64_t s;

    s = uint_add((1ULL<<24)-1, (1ULL<<24)-1, 24, &over); assert(over);
    s = uint_add((1ULL<<24)-1, 1, 24, &over); assert(over);
    s = uint_add((1ULL<<24)-1, 0, 24, &over); assert(!over && s == (1ULL<<24)-1);
    s = uint_add(0, 1, 24, &over); assert(!over && s == 1);
    s = uint_add(0, 0, 24, &over); assert(!over && s == 0);
    s = uint_sub(0, 0, 24, &over); assert(!over && s == 0);
    s = uint_sub(0, 1, 24, &over); assert(over);
    s = uint_sub(0, (1ULL<<24)-1, 24, &over); assert(over);
    s = uint_sub((1ULL<<24)-1, (1ULL<<24)-1, 24, &over); assert(!over && s == 0);
}

static void test_uint32() {
    printf("%s\n", __FUNCTION__);
    test_uint_range(32);
    bool over;
    uint64_t s;

    s = uint_add((1ULL<<32)-1, (1ULL<<32)-1, 32, &over); assert(over);
    s = uint_add((1ULL<<32)-1, 1, 32, &over); assert(over);
    s = uint_add((1ULL<<32)-1, 0, 32, &over); assert(!over && s == (1ULL<<32)-1);
    s = uint_add(0, 1, 32, &over); assert(!over && s == 1);
    s = uint_add(0, 0, 32, &over); assert(!over && s == 0);
    s = uint_sub(0, 0, 32, &over); assert(!over && s == 0);
    s = uint_sub(0, 1, 32, &over); assert(over);
    s = uint_sub(0, (1ULL<<32)-1, 32, &over); assert(over);
    s = uint_sub((1ULL<<32)-1, (1ULL<<32)-1, 32, &over); assert(!over && s == 0);
}

static void test_uint64() {
    printf("%s\n", __FUNCTION__);
    test_uint_range(64);
    bool over;
    uint64_t s;

    s = uint_add(~0ULL, ~0ULL, 64, &over); assert(over);
    s = uint_add(~0ULL, 1, 64, &over); assert(over);
    s = uint_add(~0ULL, 0, 64, &over); assert(!over && s == ~0ULL);
    s = uint_add(0, 1, 64, &over); assert(!over && s == 1);
    s = uint_add(0, 0, 64, &over); assert(!over && s == 0);
    s = uint_sub(0, 0, 64, &over); assert(!over && s == 0);
    s = uint_sub(0, 1, 64, &over); assert(over);
    s = uint_sub(0, ~0ULL, 64, &over); assert(over);
    s = uint_sub(~0ULL, ~0ULL, 64, &over); assert(!over && s == 0);
}

int main() {
    test_uint8();
    test_uint16();
    test_uint24();
    test_uint32();
    test_uint64();
    return 0;
}

