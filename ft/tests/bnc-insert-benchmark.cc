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

#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include "test.h"


#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif
const double USECS_PER_SEC = 1000000.0;

static int
long_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    const long *CAST_FROM_VOIDP(x, a->data);
    const long *CAST_FROM_VOIDP(y, b->data);
    return (*x > *y) - (*x < *y);
}

static void
run_test(unsigned long eltsize, unsigned long nodesize, unsigned long repeat)
{
    int cur = 0;
    long keys[1024];
    char *vals[1024];
    for (int i = 0; i < 1024; ++i) {
        keys[i] = rand();
        XMALLOC_N(eltsize - (sizeof keys[i]), vals[i]);
        unsigned int j = 0;
        char *val = vals[i];
        for (; j < eltsize - (sizeof keys[i]) - sizeof(int); j += sizeof(int)) {
            int *p = cast_to_typeof(p) &val[j];
            *p = rand();
        }
        for (; j < eltsize - (sizeof keys[i]); ++j) {
            char *p = &val[j];
            *p = (rand() & 0xff);
        }
    }
    XIDS xids_0 = toku_xids_get_root_xids();
    XIDS xids_123;
    int r = toku_xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);

    NONLEAF_CHILDINFO bnc;
    long long unsigned nbytesinserted = 0;
    struct timeval t[2];
    gettimeofday(&t[0], NULL);

    toku::comparator cmp;
    cmp.create(long_key_cmp, nullptr);

    for (unsigned int i = 0; i < repeat; ++i) {
        bnc = toku_create_empty_nl();
        for (; toku_bnc_nbytesinbuf(bnc) <= nodesize; ++cur) {
            toku_bnc_insert_msg(bnc,
                                &keys[cur % 1024], sizeof keys[cur % 1024],
                                vals[cur % 1024], eltsize - (sizeof keys[cur % 1024]),
                                FT_NONE, next_dummymsn(), xids_123, true,
                                cmp); assert_zero(r);
        }
        nbytesinserted += toku_bnc_nbytesinbuf(bnc);
        destroy_nonleaf_childinfo(bnc);
    }

    gettimeofday(&t[1], NULL);
    double dt;
    dt = (t[1].tv_sec - t[0].tv_sec) + ((t[1].tv_usec - t[0].tv_usec) / USECS_PER_SEC);
    double mbrate = ((double) nbytesinserted / (1 << 20)) / dt;
    long long unsigned eltrate = (long) (cur / dt);
    printf("%0.03lf MB/sec\n", mbrate);
    printf("%llu elts/sec\n", eltrate);

    cmp.destroy();
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    unsigned long eltsize, nodesize, repeat;

    initialize_dummymsn();
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <eltsize> <nodesize> <repeat>\n", argv[0]);
        return 2;
    }
    eltsize = strtoul(argv[1], NULL, 0);
    nodesize = strtoul(argv[2], NULL, 0);
    repeat = strtoul(argv[3], NULL, 0);

    run_test(eltsize, nodesize, repeat);

    return 0;
}
