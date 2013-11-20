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

enum cursor_type {
    FIRST,
    LAST,
    NEXT,
    PREV,
    CURRENT,
    SET,
    SET_RANGE,
    SET_RANGE_REVERSE
};

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

static void verify_value_by_key(DBT * key, DBT * value)
{
    assert(key->size == sizeof(int));
    assert(value->size == sizeof(int));

    int expected;
    DBT expected_dbt;
    expected_dbt.data = &expected;
    expected_dbt.size = sizeof(int);
    get_value_by_key(key, &expected_dbt);

    int * CAST_FROM_VOIDP(v, value->data);
    assert(*v == expected);
}

/**
 * Callback for cursors, can be either traversing
 * forward, backward, or not at all.
 */
struct cursor_cb_info {
    int last_key_seen;
    enum cursor_type type;
};
static int cursor_cb(DBT const * key,
        DBT const * value, void * extra)
{
    struct cursor_cb_info * CAST_FROM_VOIDP(info, extra);
    int * CAST_FROM_VOIDP(kbuf, key->data);
    int k = ntohl(*kbuf);

    switch (info->type) {
        // point queries, just verify the pair
        // is correct.
        case SET:
        case SET_RANGE:
        case SET_RANGE_REVERSE:
        case FIRST:
        case LAST:
        case CURRENT:
            verify_value_by_key((DBT *) key, (DBT *) value);
            break;
        case NEXT:
            // verify the last key we saw was the previous
            verify_value_by_key((DBT *) key, (DBT *) value);
            if (k < 0) {
                assert(k == info->last_key_seen - 1);
            }
            break;
        case PREV:
            // verify the last key we saw was the next
            verify_value_by_key((DBT *) key, (DBT *) value);
            if (k < info->last_key_seen) {
                assert(k == info->last_key_seen - 1);
            }
            break;
        default:
            assert(0);
    }

    info->last_key_seen = k;
    return TOKUDB_CURSOR_CONTINUE;
}

/**
 * Fill a FT with the the given number of rows.
 */
static void fill_db(DB_ENV * env, DB * db, int num_rows)
{
    int r;
    DB_TXN * txn;
    DBT key, value;

    printf("filling db\n");

    int i, j;
    const int ins_per_txn = 1000;
    assert(num_rows % ins_per_txn == 0);
    for (i = 0; i < num_rows; i+= ins_per_txn) {
        r = env->txn_begin(env, NULL, &txn, 0); { int chk_r = r; CKERR(chk_r); }
        for (j = 0; j < ins_per_txn && (i + j) < num_rows; j++) {
            int v, k = toku_htonl(i + j);
            dbt_init(&key, &k, sizeof(int));
            dbt_init(&value, &v, sizeof(int));
            get_value_by_key(&key, &value);
            r = db->put(db, txn, &key, &value, 0); { int chk_r = r; CKERR(chk_r); }
        }
        r = txn->commit(txn, 0); { int chk_r = r; CKERR(chk_r); }
    }
}

static void init_env(DB_ENV ** env, size_t ct_size)
{
    int r;
    const int envflags = DB_INIT_MPOOL | DB_CREATE | DB_THREAD |
        DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN | DB_PRIVATE;

    printf("initializing environment\n");

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, 0755); { int chk_r = r; CKERR(chk_r); }

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
    r = env->txn_begin(env, NULL, &txn, 0); { int chk_r = r; CKERR(chk_r); }
    r = (*db)->open(*db, txn, "db", NULL, DB_BTREE, DB_CREATE, 0644); { int chk_r = r; CKERR(chk_r); }
    r = txn->commit(txn, 0); { int chk_r = r; CKERR(chk_r); }
}

static void cleanup_env_and_db(DB_ENV * env, DB * db)
{
    int r;

    printf("cleaning up environment and db\n");
    r = db->close(db, 0); { int chk_r = r; CKERR(chk_r); }
    r = env->close(env, 0); { int chk_r = r; CKERR(chk_r); }
}

static void do_test(size_t ct_size, int num_keys)
{
    int i, r;
    DB * db;
    DB_ENV * env;

    printf("doing tests for ct_size %lu, num_keys %d\n",
            ct_size, num_keys);

    // initialize everything and insert data
    init_env(&env, ct_size);
    assert(env != NULL);
    init_db(env, &db);
    assert(db != NULL);
    fill_db(env, db, num_keys);
    
    const int last_key = num_keys - 1;

    // test c_getf_first
    printf("testing c getf first\n");
    {
        DBC * dbc;
        DB_TXN * txn;
        struct cursor_cb_info info;
        r = env->txn_begin(env, NULL, &txn, 0); { int chk_r = r; CKERR(chk_r); }
        r = db->cursor(db, txn, &dbc, 0); { int chk_r = r; CKERR(chk_r); }
        info.last_key_seen = -1;
        info.type = FIRST;
        r = dbc->c_getf_first(dbc, 0, cursor_cb, &info); { int chk_r = r; CKERR(chk_r); }
        assert(info.last_key_seen == 0);
        r = dbc->c_close(dbc); { int chk_r = r; CKERR(chk_r); }
        r = txn->commit(txn, 0); { int chk_r = r; CKERR(chk_r); }
    }

    // test c_getf_last
    printf("testing c getf last\n");
    {
        DBC * dbc;
        DB_TXN * txn;
        struct cursor_cb_info info;
        r = env->txn_begin(env, NULL, &txn, 0); { int chk_r = r; CKERR(chk_r); }
        r = db->cursor(db, txn, &dbc, 0); { int chk_r = r; CKERR(chk_r); }
        info.last_key_seen = -1;
        info.type = LAST;
        r = dbc->c_getf_last(dbc, 0, cursor_cb, &info); { int chk_r = r; CKERR(chk_r); }
        assert(info.last_key_seen == last_key);
        r = dbc->c_close(dbc); { int chk_r = r; CKERR(chk_r); }
        r = txn->commit(txn, 0); { int chk_r = r; CKERR(chk_r); }
    }

    // test c_getf_next
    printf("testing c getf next\n");
    {
        DBC * dbc;
        DB_TXN * txn;
        struct cursor_cb_info info;
        r = env->txn_begin(env, NULL, &txn, 0); { int chk_r = r; CKERR(chk_r); }
        r = db->cursor(db, txn, &dbc, 0); { int chk_r = r; CKERR(chk_r); }
        info.last_key_seen = -1;
        //info.type = FIRST;
        //r = dbc->c_getf_first(dbc, 0, cursor_cb, &info); { int chk_r =
        //r; CKERR(chk_r); }
        //assert(info.last_key_seen == 0);
        info.type = NEXT;
        while ((r = dbc->c_getf_next(dbc, 0, cursor_cb, &info)) == 0);
        assert(r == DB_NOTFOUND);
        if (info.last_key_seen != last_key) {
            printf("last keen seen %d, wanted %d\n", 
                    info.last_key_seen, last_key);
        }
        assert(info.last_key_seen == last_key);
        r = dbc->c_close(dbc); { int chk_r = r; CKERR(chk_r); }
        r = txn->commit(txn, 0); { int chk_r = r; CKERR(chk_r); }
    }

    // test c_getf_prev
    printf("testing c getf prev\n");
    {
        DBC * dbc;
        DB_TXN * txn;
        struct cursor_cb_info info;
        r = env->txn_begin(env, NULL, &txn, 0); { int chk_r = r; CKERR(chk_r); }
        r = db->cursor(db, txn, &dbc, 0); { int chk_r = r; CKERR(chk_r); }
        info.last_key_seen = -1;
        //info.type = LAST;
        //r = dbc->c_getf_last(dbc, 0, cursor_cb, &info); { int chk_r = r;
        //CKERR(chk_r); }
        //assert(info.last_key_seen == last_key);
        info.type = PREV;
        while ((r = dbc->c_getf_prev(dbc, 0, cursor_cb, &info)) == 0);
        assert(r == DB_NOTFOUND);
        assert(info.last_key_seen == 0);
        r = dbc->c_close(dbc); { int chk_r = r; CKERR(chk_r); }
        r = txn->commit(txn, 0); { int chk_r = r; CKERR(chk_r); }
    }

    printf("testing db->get, c getf set, current\n");
    {
        DBC * dbc;
        DB_TXN * txn;
        struct cursor_cb_info info;
        r = env->txn_begin(env, NULL, &txn, 0); { int chk_r = r; CKERR(chk_r); } 
        r = db->cursor(db, txn, &dbc, 0); { int chk_r = r; CKERR(chk_r); }
        for (i = 0; i < 1000; i++) {
            DBT key;
            int k = random() % num_keys;
            int nk = toku_htonl(k);
            dbt_init(&key, &nk, sizeof(int));
            
            // test c_getf_set
            info.last_key_seen = -1;
            info.type = SET;
            r = dbc->c_getf_set(dbc, 0, &key, cursor_cb, &info); { int chk_r = r; CKERR(chk_r); }
            assert(info.last_key_seen == k);

            // test c_getf_current
            info.last_key_seen = -1;
            info.type = CURRENT;
            r = dbc->c_getf_current(dbc, 0, cursor_cb, &info); { int chk_r = r; CKERR(chk_r); }
            assert(info.last_key_seen == k);

            // test db->get (point query)
            DBT value;
            memset(&value, 0, sizeof(DBT));
            r = db->get(db, txn, &key, &value, 0); { int chk_r = r; CKERR(chk_r); }
            verify_value_by_key(&key, &value);
        }
        r = dbc->c_close(dbc); { int chk_r = r; CKERR(chk_r); }
        r = txn->commit(txn, 0); { int chk_r = r; CKERR(chk_r); }
    }

    // delete some elements over a variable stride,
    // this will let us test range/reverse
    const int stride = num_keys / 10;
    printf("deleting some elements in stride %d\n", stride);
    {
        DBC * dbc;
        DB_TXN * txn;
        DBT key;
        r = env->txn_begin(env, NULL, &txn, 0); { int chk_r = r; CKERR(chk_r); } 
        r = db->cursor(db, txn, &dbc, 0); { int chk_r = r; CKERR(chk_r); }
        for (i = 0; i < num_keys; i += stride) {
            int k = toku_htonl(i);
            dbt_init(&key, &k, sizeof(int));
            r = db->del(db, txn, &key, 0);
        }
        r = dbc->c_close(dbc); { int chk_r = r; CKERR(chk_r); }
        r = txn->commit(txn, 0); { int chk_r = r; CKERR(chk_r); }
    }

    // test getf set range and range reverse
    printf("testing getf set range and range reverse\n");
    {
        DBC * dbc;
        DB_TXN * txn;
        DBT key;
        struct cursor_cb_info info;
        r = env->txn_begin(env, NULL, &txn, 0); { int chk_r = r; CKERR(chk_r); } 
        r = db->cursor(db, txn, &dbc, 0); { int chk_r = r; CKERR(chk_r); }
        for (i = 0; i < num_keys; i += stride) {
            int k = toku_htonl(i);
            dbt_init(&key, &k, sizeof(int));
            
            // we should have only actually seen the next
            // key after i if i was not the last key,
            // otherwise there's nothing after that key
            info.last_key_seen = -1;
            info.type = SET_RANGE;
            r = dbc->c_getf_set_range(dbc, 0, &key, cursor_cb, &info);
            if (i == last_key) {
                assert(r == DB_NOTFOUND);
            } else {
                assert(info.last_key_seen == i + 1);
            }

            // we should have only actually seen the prev
            // key if i was not the first key, otherwise
            // there's nothing before that key.
            info.last_key_seen = -1;
            info.type = SET_RANGE_REVERSE;
            r = dbc->c_getf_set_range_reverse(dbc, 0, &key, cursor_cb, &info);
            if (i == 0) {
                assert(r == DB_NOTFOUND);
            } else {
                assert(info.last_key_seen == i - 1);
                { int chk_r = r; CKERR(chk_r); }
            }
        }
        r = dbc->c_close(dbc); { int chk_r = r; CKERR(chk_r); }
        r = txn->commit(txn, 0); { int chk_r = r; CKERR(chk_r); }
    }

    cleanup_env_and_db(env, db);
}

int test_main(int argc, char * const argv[])
{
    default_parse_args(argc, argv);

    // just a leaf, fits in cachetable
    do_test(1L*1024*1024, 1000);
    // with internal nodes, fits in cachetable
    do_test(4L*1024*1024, 100000);
    // with internal nodes, does not fit in cachetable
    do_test(1L*1024*1024, 1000000);

    return 0;
}

