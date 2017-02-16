/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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

#include "test.h"

#include "util/dbt.h"

static void test_memcmp_magic(void) {
    int r;

    DB_ENV *env;
    r = db_env_create(&env, 0); CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL+DB_INIT_TXN, 0); CKERR(r);

    DB *db;
    r = db_create(&db, env, 0); CKERR(r);

    // Can't set the memcmp magic to 0 (since it's used as a sentinel for `none')
    r = db->set_memcmp_magic(db, 0); CKERR2(r, EINVAL);

    // Should be ok to set it more than once, even to different things, before opening.
    r = db->set_memcmp_magic(db, 1); CKERR(r);
    r = db->set_memcmp_magic(db, 2); CKERR(r);
    r = db->open(db, NULL, "db", "db", DB_BTREE, DB_CREATE, 0666); CKERR(r);

    // Can't set the memcmp magic after opening.
    r = db->set_memcmp_magic(db, 0); CKERR2(r, EINVAL);
    r = db->set_memcmp_magic(db, 1); CKERR2(r, EINVAL);

    DB *db2;
    r = db_create(&db2, env, 0); CKERR(r);
    r = db2->set_memcmp_magic(db2, 3); CKERR(r); // ..we can try setting it to something different
    // ..but it should fail to open
    r = db2->open(db2, NULL, "db", "db", DB_BTREE, DB_CREATE, 0666); CKERR2(r, EINVAL);
    r = db2->set_memcmp_magic(db2, 2); CKERR(r);
    r = db2->open(db2, NULL, "db", "db", DB_BTREE, DB_CREATE, 0666); CKERR(r);

    r = db2->close(db2, 0);
    r = db->close(db, 0); CKERR(r);

    // dbremove opens its own handle internally. ensure that the open
    // operation succeeds (and so does dbremove) despite the fact the
    // internal open does not set the memcmp magic
    r = env->dbremove(env, NULL, "db", "db", 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

static int comparison_function_unused(DB *UU(db), const DBT *UU(a), const DBT *UU(b)) {
    // We're testing that the memcmp magic gets used so the real
    // comparison function should never get called.
    invariant(false);
    return 0;
}

static int getf_key_cb(const DBT *key, const DBT *UU(val), void *extra) {
    DBT *dbt = reinterpret_cast<DBT *>(extra);
    toku_clone_dbt(dbt, *key);
    return 0;
}

static void test_memcmp_magic_sort_order(void) {
    int r;

    // Verify that randomly generated integer keys are sorted in memcmp
    // order when packed as little endian, even with an environment-wide
    // comparison function that sorts as though keys are big-endian ints.

    DB_ENV *env;
    r = db_env_create(&env, 0); CKERR(r);
    r = env->set_default_bt_compare(env, comparison_function_unused); CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL+DB_INIT_TXN, 0); CKERR(r);

    const int magic = 49;

    DB *db;
    r = db_create(&db, env, 0); CKERR(r);
    r = db->set_memcmp_magic(db, magic); CKERR(r);
    r = db->open(db, NULL, "db", "db", DB_BTREE, DB_CREATE, 0666); CKERR(r);

    for (int i = 0; i < 10000; i++) {
        char buf[1 + sizeof(int)];
        // Serialize key to first have the magic byte, then the little-endian key.
        int k = toku_htonl(random());
        buf[0] = magic;
        memcpy(&buf[1], &k, sizeof(int));

        DBT key;
        dbt_init(&key, buf, sizeof(buf));
        r = db->put(db, NULL, &key, &key, 0); CKERR(r);
    }

    DB_TXN *txn;
    env->txn_begin(env, NULL, &txn, 0);
    DBC *dbc;
    db->cursor(db, txn, &dbc, 0);
    DBT prev_dbt, curr_dbt;
    memset(&curr_dbt, 0, sizeof(DBT));
    memset(&prev_dbt, 0, sizeof(DBT));
    while (dbc->c_getf_next(dbc, 0, getf_key_cb, &curr_dbt)) {
        invariant(curr_dbt.size == sizeof(int));
        if (prev_dbt.data != NULL) {
            // Each key should be >= to the last using memcmp
            int c = memcmp(prev_dbt.data, curr_dbt.data, sizeof(int));
            invariant(c <= 0);
        }
        toku_destroy_dbt(&prev_dbt);
        prev_dbt = curr_dbt;
    }
    toku_destroy_dbt(&curr_dbt);
    toku_destroy_dbt(&prev_dbt);
    dbc->c_close(dbc);
    txn->commit(txn, 0);

    r = db->close(db, 0); CKERR(r);

    // dbremove opens its own handle internally. ensure that the open
    // operation succeeds (and so does dbremove) despite the fact the
    // internal open does not set the memcmp magic
    r = env->dbremove(env, NULL, "db", "db", 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    int r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    test_memcmp_magic();
    test_memcmp_magic_sort_order();

    return 0;
}
