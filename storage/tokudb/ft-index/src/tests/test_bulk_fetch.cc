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
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>


static void 
verify_val(DBT const *a, DBT  const *b, void *c) {
    assert(a->size == sizeof(uint64_t));
    assert(b->size == sizeof(uint64_t));
    uint64_t* expected = (uint64_t *)c;
    assert(*expected == *(uint64_t *)a->data);
    assert(*expected == *(uint64_t *)b->data);
}

static int
verify_fwd_fast(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    uint64_t* expected = (uint64_t *)c;
    *expected = *expected + 1;
    return TOKUDB_CURSOR_CONTINUE;
}

static int
verify_fwd_slow(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    uint64_t* expected = (uint64_t *)c;
    *expected = *expected + 1;
    return 0;
}

static int
verify_bwd_fast(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    uint64_t* expected = (uint64_t *)c;
    *expected = *expected - 1;
    return TOKUDB_CURSOR_CONTINUE;
}

static int
verify_bwd_slow(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    uint64_t* expected = (uint64_t *)c;
    *expected = *expected - 1;
    return 0;
}

uint64_t num_pivots_fetched_prefetch;
uint64_t num_basements_decompressed_aggressive;
uint64_t num_basements_decompressed_prefetch;
uint64_t num_basements_fetched_aggressive;
uint64_t num_basements_fetched_prefetch;

static void
init_eng_stat_vars(DB_ENV* env) {
    num_pivots_fetched_prefetch = get_engine_status_val(env, "FT_NUM_PIVOTS_FETCHED_PREFETCH");
    num_basements_decompressed_aggressive = get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_AGGRESSIVE");
    num_basements_decompressed_prefetch = get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_PREFETCH");
    num_basements_fetched_aggressive = get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_AGGRESSIVE");
    num_basements_fetched_prefetch = get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_PREFETCH");
}

static void
check_eng_stat_vars_unchanged(DB_ENV* env) {
    assert(num_pivots_fetched_prefetch == get_engine_status_val(env, "FT_NUM_PIVOTS_FETCHED_PREFETCH"));
    assert(num_basements_decompressed_aggressive == get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_AGGRESSIVE"));
    assert(num_basements_decompressed_prefetch == get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_PREFETCH"));
    assert(num_basements_fetched_aggressive == get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_AGGRESSIVE"));
    assert(num_basements_fetched_prefetch == get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_PREFETCH"));
}

static void
print_relevant_eng_stat_vars(DB_ENV* env) {
    printf("num_pivots_fetched_prefetch %" PRId64 " \n", get_engine_status_val(env, "FT_NUM_PIVOTS_FETCHED_PREFETCH"));
    printf("num_basements_decompressed_aggressive %" PRId64 " \n", get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_AGGRESSIVE"));
    printf("num_basements_decompressed_prefetch %" PRId64 " \n", get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_PREFETCH"));
    printf("num_basements_fetched_aggressive %" PRId64 " \n", get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_AGGRESSIVE"));
    printf("num_basements_fetched_prefetch %" PRId64 " \n", get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_PREFETCH"));
}

static void
test_bulk_fetch (uint64_t n, bool prelock, bool disable_prefetching) {
    if (verbose) printf("test_rand_insert:%" PRId64 " \n", n);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.bulk_fetch.ft_handle";
    int r;
    

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r=env->set_default_bt_compare(env, int64_dbt_cmp); CKERR(r);
    // arbitrarily have cachetable size be 4*n
    // goal is to make it small enough such that all of data 
    // does not fit in cachetable, but not so small that we get thrashing
    r = env->set_cachesize(env, 0, (uint32_t)4*n, 1); assert(r == 0);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, 0);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->set_readpagesize(db, 1024);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    uint64_t keys[n];
    uint64_t i;
    for (i=0; i<n; i++) {
        keys[i] = i;
    }
    
    for (i=0; i<n; i++) {
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &keys[i], sizeof keys[i]), dbt_init(&val, &i, sizeof i), 0);
        assert(r == 0);
    } 

    //
    // data inserted, now verify that using TOKUDB_CURSOR_CONTINUE in the callback works
    //
    DBC* cursor;

    // verify fast
    uint32_t flags = disable_prefetching ? DBC_DISABLE_PREFETCHING : 0;
    if (disable_prefetching) {
        init_eng_stat_vars(env);
    }
    r = db->cursor(db, NULL, &cursor, flags);
    CKERR(r);
    if (prelock) {
        r = cursor->c_set_bounds(
            cursor,
            db->dbt_neg_infty(),
            db->dbt_pos_infty(),
            true,
            0
            );
        CKERR(r);
    }
    uint64_t expected = 0;
    while (r != DB_NOTFOUND) {
        r = cursor->c_getf_next(cursor, 0, verify_fwd_fast, &expected);
        assert(r==0 || r==DB_NOTFOUND);
    }
    r = cursor->c_close(cursor); CKERR(r);
    if (disable_prefetching) {
        check_eng_stat_vars_unchanged(env);
    }
    if (verbose) {
        print_relevant_eng_stat_vars(env);
    }

    // verify slow
    if (disable_prefetching) {
        init_eng_stat_vars(env);
    }
    r = db->cursor(db, NULL, &cursor, flags);
    CKERR(r);
    if (prelock) {
        r = cursor->c_set_bounds(
            cursor,
            db->dbt_neg_infty(),
            db->dbt_pos_infty(),
            true,
            0
            );
        CKERR(r);
    }
    expected = 0;
    while (r != DB_NOTFOUND) {
        r = cursor->c_getf_next(cursor, 0, verify_fwd_slow, &expected);
        assert(r==0 || r==DB_NOTFOUND);
    }
    r = cursor->c_close(cursor); CKERR(r);
    if (disable_prefetching) {
        check_eng_stat_vars_unchanged(env);
    }
    if (verbose) {
        print_relevant_eng_stat_vars(env);
    }

    // now do backwards
    if (disable_prefetching) {
        init_eng_stat_vars(env);
    }
    r = db->cursor(db, NULL, &cursor, flags);
    CKERR(r);
    if (prelock) {
        r = cursor->c_set_bounds(
            cursor,
            db->dbt_neg_infty(),
            db->dbt_pos_infty(),
            true,
            0
            );
        CKERR(r);
    }
    expected = n-1;
    while (r != DB_NOTFOUND) {
        r = cursor->c_getf_prev(cursor, 0, verify_bwd_fast, &expected);
        assert(r==0 || r==DB_NOTFOUND);
    }
    r = cursor->c_close(cursor); CKERR(r);
    if (disable_prefetching) {
        check_eng_stat_vars_unchanged(env);
    }
    if (verbose) {
        print_relevant_eng_stat_vars(env);
    }

    // verify slow
    if (disable_prefetching) {
        init_eng_stat_vars(env);
    }
    r = db->cursor(db, NULL, &cursor, flags);
    CKERR(r);
    if (prelock) {
        r = cursor->c_set_bounds(
            cursor,
            db->dbt_neg_infty(),
            db->dbt_pos_infty(),
            true,
            0
            );
        CKERR(r);
    }
    expected = n-1;
    while (r != DB_NOTFOUND) {
        r = cursor->c_getf_prev(cursor, 0, verify_bwd_slow, &expected);
        assert(r==0 || r==DB_NOTFOUND);
    }
    r = cursor->c_close(cursor); CKERR(r);
    if (disable_prefetching) {
        check_eng_stat_vars_unchanged(env);
    }
    if (verbose) {
        print_relevant_eng_stat_vars(env);
    }


    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    test_bulk_fetch(10000, false, true);
    test_bulk_fetch(10000, true, true);
    test_bulk_fetch(10000, false, false);
    test_bulk_fetch(10000, true, false);
    return 0;
}
