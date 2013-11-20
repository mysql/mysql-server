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

#include <util/threadpool.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int verbose = 0;

static int usage(int poolsize) {
    fprintf(stderr, "[-q] [-v] [--verbose] (%d)\n", verbose);
    fprintf(stderr, "[--poolsize %d]\n", poolsize);
    return 1;
}

static void *f(void *arg) {
    return arg;
}

static void dotest(int poolsize, int nloops) {
    int r;
    struct toku_thread_pool *pool = NULL;
    r = toku_thread_pool_create(&pool, poolsize);
    assert(r == 0 && pool != NULL);

    int i;
    for (i = 0; i < nloops; i++) {
        int n = 1;
        r = toku_thread_pool_run(pool, 1, &n, f, NULL);
        assert(r == 0);
    }

    if (verbose)
        toku_thread_pool_print(pool, stderr);
    toku_thread_pool_destroy(&pool);
}

int main(int argc, char *argv[]) {
    // defaults
    int poolsize = 1;
    int nloops = 100000;

    // options
    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (arg[0] != '-')
            break;
        if (strcmp(arg, "--poolsize") == 0 && i+1 < argc) {
            poolsize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            verbose = verbose+1;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = verbose > 0 ? verbose-1 : 0;
            continue;
        }

        return usage(poolsize);
    }
    int starti = i;

    if (starti == argc) {
        dotest(poolsize, nloops);
    } else {
        for (i = starti; i < argc; i++) {
            nloops = atoi(argv[i]);
            dotest(poolsize, nloops);
        }
    }
    return 0;
}
