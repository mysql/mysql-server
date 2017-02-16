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

// Create a lot of dirty nodes, kick off a checkpoint, and close the environment.
// Measure the time it takes to close the environment since we are speeding up that
// function.

#include "test.h"
#include <toku_time.h>

// Insert max_rows key/val pairs into the db
static void do_inserts(DB_ENV *env, DB *db, uint64_t max_rows, size_t val_size) {
    char val_data[val_size]; memset(val_data, 0, val_size);
    int r;
    DB_TXN *txn = nullptr;
    r = env->txn_begin(env, nullptr, &txn, 0);
    CKERR(r);

    for (uint64_t i = 1; i <= max_rows; i++) {
        // pick a sequential key but it does not matter for this test.
        uint64_t k[2] = {
            htonl(i), random64(),
        };
        DBT key = { .data = k, .size = sizeof k };
        DBT val = { .data = val_data, .size = (uint32_t) val_size };
        r = db->put(db, txn, &key, &val, 0);
        CKERR(r);

        if ((i % 1000) == 0) {
            if (verbose)
                fprintf(stderr, "put %" PRIu64 "\n", i);
            r = txn->commit(txn, 0);
            CKERR(r);
            r = env->txn_begin(env, nullptr, &txn, 0);
            CKERR(r);
        }
    }

    r = txn->commit(txn, 0);
    CKERR(r);
}

// Create a cache with a lot of dirty nodes, kick off a checkpoint, and measure the time to
// close the environment.
static void big_shutdown(void) {
    int r;

    DB_ENV *env = nullptr;
    r = db_env_create(&env, 0);
    CKERR(r);
    r = env->set_cachesize(env, 8, 0, 1);
    CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME,
                  DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE,
                  S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    DB *db = nullptr;
    r = db_create(&db, env, 0);
    CKERR(r);
    r = db->open(db, nullptr, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    do_inserts(env, db, 1000000, 1024);

    // kick the checkpoint thread
    if (verbose)
        fprintf(stderr, "env->checkpointing_set_period\n");
    r = env->checkpointing_set_period(env, 2);
    CKERR(r);
    sleep(3);

    if (verbose)
        fprintf(stderr, "db->close\n");
    r = db->close(db, 0);
    CKERR(r);

    // measure the shutdown time
    uint64_t tstart = toku_current_time_microsec();
    if (verbose)
        fprintf(stderr, "env->close\n");
    r = env->close(env, 0);
    CKERR(r);
    uint64_t tend = toku_current_time_microsec();
    if (verbose)
        fprintf(stderr, "env->close complete %" PRIu64 " sec\n", (tend - tstart)/1000000);
}

int test_main (int argc, char *const argv[]) {
    default_parse_args(argc, argv);

    // init the env directory
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    int r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    // run the test
    big_shutdown();

    return 0;
}
