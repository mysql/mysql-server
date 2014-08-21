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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <db.h>


static uint64_t
size_from (uint32_t gbytes, uint32_t bytes) {
    return ((uint64_t)gbytes << 30) + bytes;
}

static inline void
size_to (uint64_t s, uint32_t *gbytes, uint32_t *bytes) {
    *gbytes = s >> 30;
    *bytes = s & ((1<<30) - 1);
}

static inline void
expect_le (uint64_t a, uint32_t gbytes, uint32_t bytes) {
    uint64_t b = size_from(gbytes, bytes);
    if (a != b && verbose)
        printf("WARNING: expect %" PRIu64 " got %" PRIu64 "\n", a, b);
    assert(a <= b);
}
 

static void
test_cachesize (void) {
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    int r;
    DB_ENV *env;
    uint32_t gbytes, bytes; int ncache;

    r = db_env_create(&env, 0); assert(r == 0);
    r = env->get_cachesize(env, &gbytes, &bytes, &ncache); assert(r == 0);
    if (verbose) printf("default %u %u %d\n", gbytes, bytes, ncache);

    r = env->set_cachesize(env, 0, 0, 1); assert(r == 0);
    r = env->get_cachesize(env, &gbytes, &bytes, &ncache); assert(r == 0);
    if (verbose) printf("minimum %u %u %d\n", gbytes, bytes, ncache);
    uint64_t minsize = size_from(gbytes, bytes);

    uint64_t s = 1; size_to(s, &gbytes, &bytes);
    while (gbytes <= 32) {
        r = env->set_cachesize(env, gbytes, bytes, ncache); 
        if (r != 0) {
            if (verbose) printf("max %u %u\n", gbytes, bytes);
            break;
        }
        assert(r == 0);
        r = env->get_cachesize(env, &gbytes, &bytes, &ncache); assert(r == 0);
        assert(ncache == 1);
        if (s <= minsize)
            expect_le(minsize, gbytes, bytes);
        else
            expect_le(s, gbytes, bytes);
        s *= 2; size_to(s, &gbytes, &bytes);
    }
    r = env->close(env, 0); assert(r == 0);
#endif
}


int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    test_cachesize();

    return 0;
}
