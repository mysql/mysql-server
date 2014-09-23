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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// measure the cost of closing db's with a full cache table

// create db 0 with txn 0
// create db's 1..N-1 with auto txn1
// fill the cache table with blocks for db 0
// close db 1..N-1 (these should be fast)
// close db 0
// abort txn 0

#include "test.h"
#include <toku_byteswap.h>

static long htonl64(long x) {
#if BYTE_ORDER == LITTLE_ENDIAN
    return bswap_64(x);
#else
#error
#endif
}

static inline float tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec - b->tv_sec) +1e-6*(a->tv_usec - b->tv_usec);
}

static void
insert_row(DB_ENV *env UU(), DB_TXN *txn, DB *db, uint64_t rowi) {
    int r;

    // generate the key
    char key_buffer[8];
    uint64_t k = htonl64(rowi);
    memcpy(key_buffer, &k, sizeof k);

    // generate the val
    char val_buffer[1024];
    memset(val_buffer, 0, sizeof val_buffer);

    DBT key = { .data = key_buffer, .size = sizeof key_buffer };
    DBT value = { .data = val_buffer, .size = sizeof val_buffer };
    //uint32_t put_flags = 0 | (txn ? (DB_PRELOCKED_FILE_READ | DB_PRELOCKED_WRITE) : 0);
    uint32_t put_flags = 0;
    r = db->put(db, txn, &key, &value, put_flags); assert_zero(r);
}

static void
populate(DB_ENV *env, DB_TXN *txn, DB *db, uint64_t nrows) {
    int r;
    struct timeval tstart;
    r = gettimeofday(&tstart, NULL); assert_zero(r);
    struct timeval tlast = tstart;

    for (uint64_t rowi = 0; rowi < nrows; rowi++) {
        insert_row(env, txn, db, rowi); 

        // maybe report performance
        uint64_t rows_per_report = 100000;
        if (((rowi + 1) % rows_per_report) == 0) {
            struct timeval tnow;
            r = gettimeofday(&tnow, NULL); assert_zero(r);
            float last_time = tdiff(&tnow, &tlast);
            float total_time = tdiff(&tnow, &tstart);
            if (verbose) {
                fprintf(stderr, "%" PRIu64 " %.3f %.0f/s %.0f/s\n", rowi + 1, last_time, rows_per_report/last_time, rowi/total_time); fflush(stderr);
            }
            tlast = tnow;
        }
    }
}

static void
run_test(DB_ENV *env, int ndbs, int do_txn, uint32_t pagesize, uint64_t nrows) {
    int r;

    DB *dbs[ndbs];
    for (int i = 0; i < ndbs; i++) {
        DB *db = NULL;
        if (verbose) {
            time_t now = time(0); fprintf(stderr, "%.24s creating %d\n", ctime(&now), i);
        }
        r = db_create(&db, env, 0); assert_zero(r);
        if (pagesize) {
            r = db->set_pagesize(db, pagesize); assert_zero(r);
        }
        DB_TXN *txn1 = NULL;
        if (do_txn) {
            r = env->txn_begin(env, NULL, &txn1, 0); assert_zero(r);
        }
        char db_filename[32]; sprintf(db_filename, "test%d", i);
        r = db->open(db, txn1, db_filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert_zero(r);
        if (do_txn) {
            r = txn1->commit(txn1, 0); assert_zero(r);
        }
        dbs[i] = db;
    }

        if (verbose) {
            time_t now = time(0); fprintf(stderr, "%.24s populating\n", ctime(&now));
        }

    DB_TXN *txn0 = NULL;
    if (do_txn) {
        r = env->txn_begin(env, NULL, &txn0, 0); assert_zero(r);
    }

    populate(env, txn0, dbs[ndbs-1], nrows);

    if (do_txn) {
        if (verbose) {
            time_t now = time(0); fprintf(stderr, "%.24s commit txn0\n", ctime(&now));
        }
        r = txn0->commit(txn0, 0); assert_zero(r);
    }

    for (int i = 0; i < ndbs; i++) {
        DB *db = dbs[i];
        if (verbose) {
            time_t now = time(0); fprintf(stderr, "%.24s closing %d\n", ctime(&now), i);
        }
        r = db->close(db, 0); assert_zero(r);
    }

    if (verbose) {
        time_t now = time(0); fprintf(stderr, "%.24s done\n", ctime(&now));
    }
}

int 
test_main(int argc, char * const argv[]) {
    const char *env_dir = "dir.shutdown.ca";
    int ndbs = 500;
    int do_txn = 1;
    uint32_t pagesize = 1024;
    uint64_t cachesize = 1000000000;
    uint64_t nrows = 50000;

    for (int i = 1; i < argc ; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(arg, "--txn") == 0 && i+1 < argc) {
            do_txn = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--ndbs") == 0 && i+1 < argc) {
            ndbs = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--pagesize") == 0 && i+1 < argc) {
            pagesize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--cachesize") == 0 && i+1 < argc) {
            cachesize = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--rows") == 0 && i+1 < argc) {
            nrows = atol(argv[++i]);
            continue;
        }

        assert(0);
    }

    // create clean env dir
    char rm_cmd[strlen(env_dir) + strlen("rm -rf ") + 1];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", env_dir);
    int r;
    r = system(rm_cmd); assert_zero(r);
    r = toku_os_mkdir(env_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); assert_zero(r);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);
    if (cachesize) {
        const uint64_t gig = 1 << 30;
        r = env->set_cachesize(env, cachesize / gig, cachesize % gig, 1); assert_zero(r);
    }
    int env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG;
    if (!do_txn)
        env_open_flags &= ~(DB_INIT_TXN | DB_INIT_LOG);
    r = env->open(env, env_dir, env_open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert_zero(r);

    run_test(env, ndbs, do_txn, pagesize, nrows);

    if (verbose) fprintf(stderr, "closing env\n");
    r = env->close(env, 0); assert_zero(r);

    return 0;
}
