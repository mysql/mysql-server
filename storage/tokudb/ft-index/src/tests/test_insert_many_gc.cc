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
  Copyright (C) 2014 Tokutek, Inc.

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

#ident "Copyright (c) 2014 Tokutek Inc.  All rights reserved."

#include "test.h"


static void test_insert_many_gc(void) {
    int r;

    DB_ENV *env;
    r = db_env_create(&env, 0); CKERR(r);
    r = env->set_cachesize(env, 1, 0, 1); CKERR(r); // 1gb cache so this test fits in memory
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL+DB_INIT_TXN, 0); CKERR(r);

    DB *db;
    r = db_create(&db, env, 0); CKERR(r);
    r = db->open(db, NULL, "db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(r);

    const int val_size = 1 * 1024 * 1024;

    // Begin a snapshot transaction, which should prevent simple garbage collection
    // from being effective. Only full garbage collection can prevent many inserts
    // into a single leaf node from growing out of control.
    DB_TXN *snapshot_txn;
    r = env->txn_begin(env, NULL, &snapshot_txn, DB_TXN_SNAPSHOT); CKERR(r);

    DBT key;
    int k = 0;
    dbt_init(&key, &k, sizeof(k));

    DBT val;
    char *XMALLOC_N(val_size, val_buf);
    memset(val_buf, 0, val_size);
    dbt_init(&val, val_buf, val_size);

    // Keep overwriting the same row over and over.
    const int N = 75;
    for (int i = 0; i < N; i++) {
        r = db->put(db, NULL, &key, &val, 0); CKERR(r);
    }

    // Full garbage collection should have prevented the leaf node
    // from having an MVCC stack of size 'N'. At the time of this
    // writing, we run full GC on leaf-inject when the leaf is
    // 32mb or larger. A good invariant is that the max LE size
    // never grew larger than 35mb and that the max commited xr stack
    // length never exceeded 35
    const uint64_t le_max_memsize = get_engine_status_val(env, "LE_MAX_MEMSIZE");
    const uint64_t le_max_committed_xr = get_engine_status_val(env, "LE_MAX_COMMITTED_XR");
    invariant(le_max_memsize <= 35 * 1024 * 1024);
    invariant(le_max_committed_xr <= 35);

    r = snapshot_txn->commit(snapshot_txn, 0); CKERR(r);

    toku_free(val_buf);
    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    int r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    test_insert_many_gc();

    return 0;
}
