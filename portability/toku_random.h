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

#ifndef TOKU_RANDOM_H
#define TOKU_RANDOM_H

#include <portability/toku_config.h>
#include <toku_portability.h>
#include <toku_assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#if defined(HAVE_RANDOM_R)
// Definition of randu62 and randu64 assume myrandom_r generates 31 low-order bits
static_assert(RAND_MAX == INT32_MAX, "Unexpected RAND_MAX");
static inline int
myinitstate_r(unsigned int seed, char *statebuf, size_t statelen, struct random_data *buf)
{
    return initstate_r(seed, statebuf, statelen, buf);
}
static inline int32_t
myrandom_r(struct random_data *buf)
{
    int32_t x;
    int r = random_r(buf, &x);
    lazy_assert_zero(r);
    return x;
}
#elif defined(HAVE_NRAND48)
struct random_data {
    unsigned short xsubi[3];
};
static inline int
myinitstate_r(unsigned int seed, char *UU(statebuf), size_t UU(statelen), struct random_data *buf)
{
    buf->xsubi[0] = (seed & 0xffff0000) >> 16;
    buf->xsubi[0] = (seed & 0x0000ffff);
    buf->xsubi[2] = (seed & 0x00ffff00) >> 8;
    return 0;
}
static inline int32_t
myrandom_r(struct random_data *buf)
{
    int32_t x = nrand48(buf->xsubi);
    return x;
}
#else
# error "no suitable reentrant random function available (checked random_r and nrand48)"
#endif

static inline uint64_t
randu62(struct random_data *buf)
{
    uint64_t a = myrandom_r(buf);
    uint64_t b = myrandom_r(buf);
    return (a | (b << 31));
}

static inline uint64_t
randu64(struct random_data *buf)
{
    uint64_t r62 = randu62(buf);
    uint64_t c = myrandom_r(buf);
    return (r62 | ((c & 0x3) << 62));
}

static inline uint32_t
rand_choices(struct random_data *buf, uint32_t choices) {
    invariant(choices >= 2);
    invariant(choices < INT32_MAX);
    uint32_t bits = 2;
    while (bits < choices) {
        bits *= 2;
    }
    --bits;

    uint32_t result;
    do {
        result = myrandom_r(buf) & bits;
    } while (result >= choices);

    return result;
}

#endif // TOKU_RANDOM_H
