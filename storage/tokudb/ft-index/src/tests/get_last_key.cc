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
/**
 * Test that various queries behave correctly
 *
 * Zardosht says:
 *
 * write a test that inserts a bunch of elements into the tree, 
 * and then verify that the following types of queries work:
 * - db->get
 * - next
 * - prev
 * - set_range
 * - set_range_reverse
 * - first
 * - last
 * - current
 *
 * do it on a table with:
 * - just a leaf node
 * - has internal nodes (make node size 4K and bn size 1K)
 * - big cachetable such that everything fits
 * - small cachetable such that not a lot fits
 * 
 * make sure APIs are the callback APIs (getf_XXX)
 * make sure your callbacks all return TOKUDB_CURSOR_CONTINUE, 
 * so we ensure that returning TOKUDB_CURSOR_CONTINUE does not 
 * mess anything up.
 */

#include "test.h"

/**
 * Calculate or verify that a value for a given key is correct
 * Returns 0 if the value is correct, nonzero otherwise.
 */
static void get_value_by_key(DBT * key, DBT * value)
{
    // keys/values are always stored in the DBT in net order
    int * CAST_FROM_VOIDP(k, key->data); 
    int v = toku_ntohl(*k) * 2 + 1;
    memcpy(value->data, &v, sizeof(int));
}

static void prepare_for_env(void) {
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    int r = toku_os_mkdir(TOKU_TEST_FILENAME, 0755); { int chk_r = r; CKERR(chk_r); }
}

static void init_env(DB_ENV ** env, size_t ct_size)
{
    int r;
    const int envflags = DB_INIT_MPOOL | DB_CREATE | DB_THREAD |
        DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN | DB_PRIVATE;

    printf("initializing environment\n");

    r = db_env_create(env, 0); { int chk_r = r; CKERR(chk_r); }
    assert(ct_size < 1024 * 1024 * 1024L);
    r = (*env)->set_cachesize(*env, 0, ct_size, 1); { int chk_r = r; CKERR(chk_r); }
    r = (*env)->open(*env, TOKU_TEST_FILENAME, envflags, 0755); { int chk_r = r; CKERR(chk_r); }
}

static void init_db(DB_ENV * env, DB ** db)
{
    int r;
    const int node_size = 4096;
    const int bn_size = 1024;

    printf("initializing db\n");

    DB_TXN * txn;
    r = db_create(db, env, 0); { int chk_r = r; CKERR(chk_r); }
    r = (*db)->set_readpagesize(*db, bn_size); { int chk_r = r; CKERR(chk_r); }
    r = (*db)->set_pagesize(*db, node_size); { int chk_r = r; CKERR(chk_r); }
    r = env->txn_begin(env, nullptr, &txn, 0); { int chk_r = r; CKERR(chk_r); }
    r = (*db)->open(*db, txn, "db", nullptr, DB_BTREE, DB_CREATE, 0644); { int chk_r = r; CKERR(chk_r); }
    r = txn->commit(txn, 0); { int chk_r = r; CKERR(chk_r); }
}

static void cleanup_env_and_db(DB_ENV * env, DB * db)
{
    int r;

    printf("cleaning up environment and db\n");
    r = db->close(db, 0); { int chk_r = r; CKERR(chk_r); }
    r = env->close(env, 0); { int chk_r = r; CKERR(chk_r); }
}

static int get_last_key_cb(const DBT *key, const DBT *value, void *extra) {
    if (key->data) {
        invariant_null(value);
        int expected_key = *(int*)extra;
        int found_key = *(int*)key->data;
        invariant(expected_key == (int)ntohl(found_key));
    }
    return 0;
}


static void check_last_key_matches(DB *db, int expect_r, int key) {
    int r = db->get_last_key(db, get_last_key_cb, &key);
    CKERR2(r, expect_r);
}

static void do_test(size_t ct_size, int num_keys)
{
    int i, r;
    DB * db;
    DB_ENV * env;
    DB_TXN *txn = nullptr;
    DB_TXN *txn2 = nullptr;
    uint64_t loops_run = 0;


    printf("doing tests for ct_size %lu, num_keys %d\n",
            ct_size, num_keys);

    // initialize everything and insert data
    prepare_for_env();
    init_env(&env, ct_size);
    assert(env != nullptr);
    init_db(env, &db);
    assert(db != nullptr);

    r = env->txn_begin(env, nullptr, &txn, 0);
    CKERR(r);
    DBT key, value;
    for (i = 0; i < num_keys; i++) {
        int v, k = toku_htonl(i);
        dbt_init(&key, &k, sizeof(int));
        dbt_init(&value, &v, sizeof(int));
        get_value_by_key(&key, &value);
        r = db->put(db, txn, &key, &value, 0);
        CKERR(r);
    }
    CKERR(r);

    int expect_r = num_keys == 0 ? DB_NOTFOUND : 0;
    check_last_key_matches(db, expect_r, num_keys - 1);

    r = txn->commit(txn, 0);
    check_last_key_matches(db, expect_r, num_keys - 1);

    if (num_keys == 0) {
        goto cleanup;
    }
    r = env->txn_begin(env, nullptr, &txn2, 0);
    CKERR(r);
    r = env->txn_begin(env, nullptr, &txn, 0);
    CKERR(r);

    r = db->del(db, txn, &key, 0);
    check_last_key_matches(db, 0, num_keys - 1);

    r = txn->commit(txn, 0);
    check_last_key_matches(db, 0, num_keys - 1);

    r = txn2->commit(txn2, 0);
    check_last_key_matches(db, 0, num_keys - 1);

    //Run Garbage collection (NOTE does not work when everything fits in root??? WHY)
    r = db->hot_optimize(db, nullptr, nullptr, nullptr, nullptr, &loops_run);
    CKERR(r);

    r = env->txn_checkpoint(env, 0, 0, 0);
    CKERR(r);

    //Run Garbage collection (NOTE does not work when everything fits in root??? WHY)
    r = db->hot_optimize(db, nullptr, nullptr, nullptr, nullptr, &loops_run);
    CKERR(r);

    r = env->txn_checkpoint(env, 0, 0, 0);
    CKERR(r);

    //Fully close and reopen
    //This clears cachetable
    //note that closing a db and reopening may not flush the cachetable so we close env as well
    cleanup_env_and_db(env, db);
    init_env(&env, ct_size);
    assert(env != nullptr);
    init_db(env, &db);
    assert(db != nullptr);

    //NOTE: tried overkill (double optimize, double checkpoint.. gc still doesn't happen for everything in root in single basement

    if (num_keys >= 2) {
        // At least one key remains.
        check_last_key_matches(db, 0, num_keys - 2);
    } else {
        //no key remains.  Should find nothing.
        check_last_key_matches(db, DB_NOTFOUND, -1);
    }
cleanup:
    cleanup_env_and_db(env, db);
}

int test_main(int argc, char * const argv[])
{
    default_parse_args(argc, argv);

    for (int i = 0; i <= 2; i++) {
        do_test(1024*1024, i);
    }
    for (int i = 4; i <= 1024; i*=2) {
        do_test(1024*1024, i);
    }

    return 0;
}

