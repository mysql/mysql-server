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
#if !defined(_TOKUDB_MATH_H)
#define _TOKUDB_MATH_H

namespace tokudb {

// Add and subtract ints with overflow detection.
// Overflow detection adapted from "Hackers Delight", Henry S. Warren

// Return a bit mask for bits 0 .. length_bits-1
static uint64_t uint_mask(uint length_bits) __attribute__((unused));
static uint64_t uint_mask(uint length_bits) {
    return length_bits == 64 ? ~0ULL : (1ULL<<length_bits)-1;
}

// Return the highest unsigned int with a given number of bits
static uint64_t uint_high_endpoint(uint length_bits) __attribute__((unused));
static uint64_t uint_high_endpoint(uint length_bits) {
    return uint_mask(length_bits);
}

// Return the lowest unsigned int with a given number of bits
static uint64_t uint_low_endpoint(uint length_bits) __attribute__((unused));
static uint64_t uint_low_endpoint(uint length_bits __attribute__((unused))) {
    return 0;
}

// Add two unsigned integers with max maximum value.
// If there is an overflow then set the sum to the max.
// Return the sum and the overflow.
static uint64_t uint_add(uint64_t x, uint64_t y, uint length_bits, bool *over) __attribute__((unused));
static uint64_t uint_add(uint64_t x, uint64_t y, uint length_bits, bool *over) {
    uint64_t mask = uint_mask(length_bits);
    assert((x & ~mask) == 0 && (y & ~mask) == 0);
    uint64_t s = (x + y) & mask;
    *over = s < x;     // check for overflow
    return s;
}

// Subtract two unsigned ints with max maximum value.
// If there is an over then set the difference to 0.
// Return the difference and the overflow.
static uint64_t uint_sub(uint64_t x, uint64_t y, uint length_bits, bool *over) __attribute__((unused));
static uint64_t uint_sub(uint64_t x, uint64_t y, uint length_bits, bool *over) {
    uint64_t mask = uint_mask(length_bits);
    assert((x & ~mask) == 0 && (y & ~mask) == 0);
    uint64_t s = (x - y) & mask;
    *over = s > x;    // check for overflow
    return s;
}

// Return the highest int with a given number of bits
static int64_t int_high_endpoint(uint length_bits) __attribute__((unused));
static int64_t int_high_endpoint(uint length_bits) {
    return (1ULL<<(length_bits-1))-1;
}

// Return the lowest int with a given number of bits
static int64_t int_low_endpoint(uint length_bits) __attribute__((unused));
static int64_t int_low_endpoint(uint length_bits) {
    int64_t mask = uint_mask(length_bits);
    return (1ULL<<(length_bits-1)) | ~mask;
}

// Sign extend to 64 bits an int with a given number of bits
static int64_t int_sign_extend(int64_t n, uint length_bits) __attribute__((unused));
static int64_t int_sign_extend(int64_t n, uint length_bits) {
    if (n & (1ULL<<(length_bits-1)))
        n |= ~uint_mask(length_bits);
    return n;
}

// Add two signed ints with max maximum value.
// If there is an overflow then set the sum to the max or the min of the int range,
// depending on the sign bit.
// Sign extend to 64 bits.
// Return the sum and the overflow.
static int64_t int_add(int64_t x, int64_t y, uint length_bits, bool *over) __attribute__((unused));
static int64_t int_add(int64_t x, int64_t y, uint length_bits, bool *over) {
    int64_t mask = uint_mask(length_bits);
    int64_t n = (x + y) & mask;
    *over = (((n ^ x) & (n ^ y)) >> (length_bits-1)) & 1;    // check for overflow
    if (n & (1LL<<(length_bits-1)))
        n |= ~mask;    // sign extend
    return n;
}

// Subtract two signed ints.
// If there is an overflow then set the sum to the max or the min of the int range,
// depending on the sign bit.
// Sign extend to 64 bits.
// Return the sum and the overflow.
static int64_t int_sub(int64_t x, int64_t y, uint length_bits, bool *over) __attribute__((unused));
static int64_t int_sub(int64_t x, int64_t y, uint length_bits, bool *over) {
    int64_t mask = uint_mask(length_bits);
    int64_t n = (x - y) & mask;
    *over = (((x ^ y) & (n ^ x)) >> (length_bits-1)) & 1;    // check for overflow
    if (n & (1LL<<(length_bits-1)))
        n |= ~mask;    // sign extend
    return n;
}

} // namespace tokudb

#endif
