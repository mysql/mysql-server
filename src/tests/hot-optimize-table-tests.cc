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
// hot-optimize-table-tests.c

#include "test.h"

const int envflags = DB_INIT_MPOOL |
                     DB_CREATE |
                     DB_THREAD |
                     DB_INIT_LOCK |
                     DB_INIT_LOG |
                     DB_INIT_TXN |
                     DB_PRIVATE;

DB_ENV* env;
unsigned int leaf_hits;

// Custom Update Function for our test FT.
static int
update_func(DB* UU(db),
            const DBT* key,
            const DBT* old_val,
            const DBT* extra,
            void (*set_val)(const DBT* new_val, void* set_extra) __attribute__((unused)),
            void* UU(set_extra))
{
    unsigned int *x_results;
    assert(extra->size == sizeof x_results);
    x_results = *(unsigned int **) extra->data;
    assert(x_results);
    assert(old_val->size > 0);
    unsigned int* indexptr;
    assert(key->size == (sizeof *indexptr));
    indexptr = (unsigned int*)key->data;
    ++leaf_hits;

    if (verbose && x_results[*indexptr] != 0) {
        printf("x_results = %p, indexptr = %p, *indexptr = %u, x_results[*indexptr] = %u\n", x_results, indexptr, *indexptr, x_results[*indexptr]);
    }

    assert(x_results[*indexptr] == 0);
    x_results[*indexptr]++;
    // ++(x_results[*indexptr]);
    // memset(&new_val, 0, sizeof(new_val));
    // set_val(&new_val, set_extra);
    unsigned int i = *indexptr;
    if (verbose && ((i + 1) % 50000 == 0)) {
        printf("applying update to %u\n", i);
        //printf("x_results[] = %u\n", x_results[*indexptr]);
    }

    return 0;
}

///
static void
hot_test_setup(void)
{
    int r = 0;
    // Remove any previous environment.
    toku_os_recursive_delete(TOKU_TEST_FILENAME);

    // Set up a new environment.
    { int chk_r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);CKERR(r);
    env->set_update(env, update_func);
    { int chk_r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

static void
hot_test_destroy(void)
{
    { int chk_r = env->close(env, 0); CKERR(chk_r); }
}

///
static void
hot_insert_keys(DB* db, unsigned int key_count)
{
    int r = 0;
    DB_TXN * xact;
    unsigned int limit = 1;
    if (key_count > 10) {
        limit = 100000;
    }

    // Dummy data.
    const unsigned int DUMMY_SIZE = 100;
    size_t size = DUMMY_SIZE;
    char* dummy = NULL;
    dummy = (char*)toku_xmalloc(size);
    memset(dummy, 0, size);

    // Start the transaction for insertions.
    //
    r = env->txn_begin(env, 0, &xact, 0); CKERR(r);

    unsigned int key;

    DBT key_thing;
    DBT *keyptr = dbt_init(&key_thing, &key, sizeof(key));
    DBT value_thing;
    DBT *valueptr = dbt_init(&value_thing, dummy, size);
    for (key = 0; key < key_count; ++key)
    {
        { int chk_r = db->put(db, xact, keyptr, valueptr, 0); CKERR(chk_r); }

        // DEBUG OUTPUT
        //
        if (verbose && (key + 1) % limit == 0) {
            printf("%d Elements inserted.\n", key + 1);
        }
    }

    // Commit the insert transaction.
    //
    r = xact->commit(xact, 0); CKERR(r);

    toku_free(dummy);
}

///
static void
hot_create_db(DB** db, const char* c)
{
    int r = 0;
    DB_TXN* xact;
    verbose ? printf("Creating DB.\n") : 0;
    r = env->txn_begin(env, 0, &xact, 0); CKERR(r);
    { int chk_r = db_create(db, env, 0); CKERR(chk_r); }
    { int chk_r = (*db)->open((*db), xact, c, NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
    r = xact->commit(xact, 0); CKERR(r);
    verbose ? printf("DB Created.\n") : 0;
}

///
static void
hot_test(DB* db, unsigned int size)
{
    int r = 0;
    leaf_hits = 0;
    verbose ? printf("Insert some data.\n") : 0;

    // Insert our keys to assemble the tree.
    hot_insert_keys(db, size);

    // Insert Broadcast Message.
    verbose ? printf("Insert Broadcast Message.\n") : 0;
    unsigned int *XMALLOC_N(size, x_results);
    memset(x_results, 0, (sizeof x_results[0]) * size);
    DBT extra;
    DBT *extrap = dbt_init(&extra, &x_results, sizeof x_results);
    DB_TXN * xact;
    r = env->txn_begin(env, 0, &xact, 0); CKERR(r);
    r = db->update_broadcast(db, xact, extrap, 0); CKERR(r);
    r = xact->commit(xact, 0); CKERR(r);

    // Flatten the tree.
    verbose ? printf("Calling hot optimize...\n") : 0;
    uint64_t loops_run;
    r = db->hot_optimize(db, NULL, NULL, NULL, NULL, &loops_run);
    assert(r == 0);
    verbose ? printf("HOT Finished!\n") : 0;
    for (unsigned int i = 0; i < size; ++i) {
        assert(x_results[i] == 1);
    }
    verbose ? printf("Leaves hit = %u\n", leaf_hits) :0;
    toku_free(x_results);
}

///
int 
test_main(int argc, char * const argv[])
{
    int r = 0;
    default_parse_args(argc, argv);
    hot_test_setup();

    // Create and Open the Database/FT
    DB *db = NULL;
    const unsigned int BIG = 4000000;
    const unsigned int SMALL = 10;
    const unsigned int NONE = 0;

    hot_create_db(&db, "none.db");
    hot_test(db, NONE);
    r = db->close(db, 0);
    CKERR(r);
    hot_create_db(&db, "small.db");
    hot_test(db, SMALL);
    r = db->close(db, 0);
    CKERR(r);
    hot_create_db(&db, "big.db");
    hot_test(db, BIG);
    r = db->close(db, 0);
    CKERR(r);

    hot_test_destroy();
    verbose ? printf("Exiting Test.\n") : 0;
    return r;
}
