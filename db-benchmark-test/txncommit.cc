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
// run txn begin commit on multiple threads and measure the throughput

#include "toku_portability.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include "db.h"
#include "toku_pthread.h"

static double now(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL); assert(r == 0);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

static void test_txn(DB_ENV *env, uint32_t ntxns, long usleep_time) {
    int r;
    for (uint32_t i = 0; i < ntxns; i++) {
        DB_TXN *txn = NULL;
        r = env->txn_begin(env, NULL, &txn, DB_TXN_SNAPSHOT); assert(r == 0);
        r = txn->commit(txn, 0); assert(r == 0);
        if (usleep_time)
            usleep(usleep_time); 
        else
            sched_yield();
    }
}

struct arg {
    DB_ENV *env;
    uint32_t ntxns;
    long usleep_time;
};

static void *test_thread(void *arg) {
    struct arg *myarg = (struct arg *) arg;
    test_txn(myarg->env, myarg->ntxns, myarg->usleep_time);
    return arg;
}

int main(int argc, char * const argv[]) {
    uint32_t ntxns = 1000000;
    uint32_t nthreads = 1;
    long usleep_time = 0;

    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "--ntxns") == 0 && i+1 < argc) {
            ntxns = atoll(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--nthreads") == 0 && i+1 < argc) {
            nthreads = atoll(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--usleep") == 0 && i+1 < argc) {
            usleep_time = atol(argv[++i]);
            continue;
        }
        return 1;
    }

    int r;

    char cmd[256];
    sprintf(cmd, "rm -rf %s", ENVDIR);
    r = system(cmd); assert(r == 0);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_cachesize(env, 4, 0, 1); assert(r == 0);
    r = env->open(env, ENVDIR, DB_PRIVATE + DB_THREAD + DB_CREATE + DB_INIT_TXN + DB_INIT_LOG + DB_INIT_MPOOL + DB_RECOVER, 0); assert(r == 0);

    double tstart = now();
    assert(nthreads > 0);
    struct arg myargs[nthreads-1];
    toku_pthread_t mytids[nthreads];
    for (uint32_t i = 0; i < nthreads-1; i++) {
        myargs[i] = (struct arg) { env, ntxns, usleep_time};
        r = toku_pthread_create(&mytids[i], NULL, test_thread, &myargs[i]); assert(r == 0);
    }
    test_txn(env, ntxns, usleep_time);
    for (uint32_t i = 0; i < nthreads-1; i++) {
        void *ret;
        r = toku_pthread_join(mytids[i], &ret); assert(r == 0);
    }
    double tend = now();
    double t = tend-tstart;
    double n = ntxns * nthreads;
    printf("%f %f %f\n", n, t, (n/t)*1000000.0);
    r = env->close(env, 0); assert(r == 0);
    return 0;
}
