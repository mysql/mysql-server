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
 * Test that unique inserts work correctly. This exercises the rightmost leaf inject optimization.
 */

#include <portability/toku_random.h>

#include "test.h"

static char random_buf[8];
static struct random_data random_data;

static void test_simple_unique_insert(DB_ENV *env) {
    int r;
    DB *db;
    r = db_create(&db, env, 0); CKERR(r);
    r = db->open(db, NULL, "db", NULL, DB_BTREE, DB_CREATE, 0644); CKERR(r);

    DBT key1, key2, key3;
    dbt_init(&key1, "a", sizeof("a"));
    dbt_init(&key2, "b", sizeof("b"));
    dbt_init(&key3, "c", sizeof("c"));
    r = db->put(db, NULL, &key1, &key1, DB_NOOVERWRITE); CKERR(r);
    r = db->put(db, NULL, &key1, &key1, DB_NOOVERWRITE); CKERR2(r, DB_KEYEXIST);
    r = db->put(db, NULL, &key3, &key3, DB_NOOVERWRITE); CKERR(r);
    r = db->put(db, NULL, &key3, &key3, DB_NOOVERWRITE); CKERR2(r, DB_KEYEXIST);
    r = db->put(db, NULL, &key2, &key2, DB_NOOVERWRITE); CKERR(r);
    r = db->put(db, NULL, &key2, &key2, DB_NOOVERWRITE); CKERR2(r, DB_KEYEXIST);
    // sanity check
    r = db->put(db, NULL, &key1, &key1, DB_NOOVERWRITE); CKERR2(r, DB_KEYEXIST);
    r = db->put(db, NULL, &key1, &key3, DB_NOOVERWRITE); CKERR2(r, DB_KEYEXIST);

    r = db->close(db, 0); CKERR(r);
    r = env->dbremove(env, NULL, "db", NULL, 0); CKERR(r);
}

static void test_large_sequential_insert_unique(DB_ENV *env) {
    int r;
    DB *db;
    r = db_create(&db, env, 0); CKERR(r);

    // very small nodes/basements to make a taller tree
    r = db->set_pagesize(db, 8 * 1024); CKERR(r);
    r = db->set_readpagesize(db, 2 * 1024); CKERR(r);
    r = db->open(db, NULL, "db", NULL, DB_BTREE, DB_CREATE, 0644); CKERR(r);

    const int val_size = 1024;
    char *XMALLOC_N(val_size, val_buf);
    memset(val_buf, 'k', val_size);
    DBT val;
    dbt_init(&val, val_buf, val_size);

    // grow a tree to about depth 3, taking sanity checks along the way
    const int start_num_rows = (64 * 1024 * 1024) / val_size;
    for (int i = 0; i < start_num_rows; i++) {
        DBT key;
        int k = toku_htonl(i);
        dbt_init(&key, &k, sizeof(k));
        r = db->put(db, NULL, &key, &val, DB_NOOVERWRITE); CKERR(r);
        if (i % 50 == 0) {
            // sanity check - should not be able to insert this key twice in a row
            r = db->put(db, NULL, &key, &val, DB_NOOVERWRITE); CKERR2(r, DB_KEYEXIST);

            // .. but re-inserting is okay, if we provisionally deleted the row
            DB_TXN *txn;
            r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
            r = db->del(db, NULL, &key, DB_DELETE_ANY); CKERR(r);
            r = db->put(db, NULL, &key, &val, DB_NOOVERWRITE); CKERR(r);
            r = txn->commit(txn, 0); CKERR(r);
        }
        if (i > 0 && i % 250 == 0) {
            // sanity check - unique checks on random keys we already inserted should
            //                fail (exercises middle-of-the-tree checks)
            for (int check_i = 0; check_i < 4; check_i++) {
                DBT rand_key;
                int rand_k = toku_htonl(myrandom_r(&random_data) % i);
                dbt_init(&rand_key, &rand_k, sizeof(rand_k));
                r = db->put(db, NULL, &rand_key, &val, DB_NOOVERWRITE); CKERR2(r, DB_KEYEXIST);
            }
        }
    }

    toku_free(val_buf);
    r = db->close(db, 0); CKERR(r);
    r = env->dbremove(env, NULL, "db", NULL, 0); CKERR(r);
}


int test_main(int argc, char * const argv[]) {
    default_parse_args(argc, argv);

    int r;
    const int envflags = DB_INIT_MPOOL | DB_CREATE | DB_THREAD |
                         DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN | DB_PRIVATE;

    // startup
    DB_ENV *env;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, 0755); CKERR(r);
    r = db_env_create(&env, 0); CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, 0755);

    r = myinitstate_r(random(), random_buf, 8, &random_data); CKERR(r);

    test_simple_unique_insert(env);
    test_large_sequential_insert_unique(env);

    // cleanup
    r = env->close(env, 0); CKERR(r);

    return 0;
}

