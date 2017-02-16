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
#include "test.h"

// verify that put_multiple inserts the correct rows into N dictionaries
// verify that pu_multiple locks the correct keys for N dictionaries

const int max_rows_per_primary = 9;

static uint32_t
get_total_secondary_rows(uint32_t num_primary) {
    assert((num_primary % (max_rows_per_primary+1)) == 0);
    return num_primary / (max_rows_per_primary+1) * 
        ( (max_rows_per_primary) * (max_rows_per_primary+1) / 2 );
}

static uint8_t
get_num_keys(uint16_t i, uint8_t dbnum) {
    return (i+dbnum) % (max_rows_per_primary + 1);  // 0..9.. 10 choices
}

static uint16_t
get_total_num_keys(uint16_t i, uint8_t num_dbs) {
    uint16_t sum = 0;
    for (uint8_t db = 0; db < num_dbs; ++db) {
        sum += get_num_keys(i, db);
    }
    return sum;
}

static uint32_t
get_key(uint16_t i, uint8_t dbnum, uint8_t which) {
    uint32_t i32 = i;
    uint32_t dbnum32 = dbnum;
    uint32_t which32 = which;
    uint32_t x = (dbnum32<<24) | (i32) | (which32<<8);
    return x;
}

static void
get_data(uint32_t *v, uint8_t i, uint8_t ndbs) {
    int index = 0;
    for (uint8_t dbnum = 0; dbnum < ndbs; dbnum++) {
        for (uint8_t which = 0; which < get_num_keys(i, dbnum); ++which) {
            v[index++] = get_key(i, dbnum, which);
        }
    }
}

static int
put_callback(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val) {
    (void) src_val;
    uint8_t dbnum;
    assert(dest_db->descriptor->dbt.size == sizeof dbnum);
    memcpy(&dbnum, dest_db->descriptor->dbt.data, sizeof dbnum);

    assert(dbnum > 0); // Does not get called for primary.
    assert(dest_db != src_db);

    assert(src_key->size == 2);
    uint16_t i = *(uint16_t*)src_key->data;
    uint8_t num_keys = get_num_keys(i, dbnum);

    toku_dbt_array_resize(dest_keys, num_keys);
    if (dest_vals) {
        toku_dbt_array_resize(dest_vals, num_keys);
    }

    for (uint8_t which = 0; which < num_keys; ++which) {
        DBT *dest_key = &dest_keys->dbts[which];

        assert(dest_key->flags == DB_DBT_REALLOC);
        {
            // Memory management
            if (dest_key->ulen < sizeof(uint32_t)) {
                dest_key->data = toku_xrealloc(dest_key->data, sizeof(uint32_t));
                dest_key->ulen = sizeof(uint32_t);
            }
            dest_key->size = sizeof(uint32_t);
        }
        *(uint32_t*)dest_key->data = get_key(i, dbnum, which);

        if (dest_vals) {
            DBT *dest_val = &dest_vals->dbts[which];
            dest_val->flags = 0;
            dest_val->data = nullptr;
            dest_val->size = 0;
        }
    }
    return 0;
}

static int
del_callback(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, const DBT *src_key, const DBT *src_data) {
    return put_callback(dest_db, src_db, dest_keys, NULL, src_key, src_data);
}

static void
verify_locked(DB_ENV *env, DB *db, uint8_t dbnum, uint16_t i) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);
    if (dbnum == 0) {
        DBT key; dbt_init(&key, &i, sizeof i);
        r = db->del(db, txn, &key, DB_DELETE_ANY); CKERR2(r, DB_LOCK_NOTGRANTED);
    } else {
        for (uint8_t which = 0; which < get_num_keys(i, dbnum); ++which) {
            uint32_t k = get_key(i, dbnum, which);
            DBT key; dbt_init(&key, &k, sizeof k);
            r = db->del(db, txn, &key, DB_DELETE_ANY); CKERR2(r, DB_LOCK_NOTGRANTED);
        }
    }
    r = txn->abort(txn); assert_zero(r);
}

static void
verify_seq_primary(DB_ENV *env, DB *db, int dbnum, int ndbs, int nrows) {
    assert(dbnum==0);
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DBC *cursor = NULL;
    r = db->cursor(db, txn, &cursor, 0); assert_zero(r);
    int i;
    for (i = 0; ; i++) {
        DBT key; memset(&key, 0, sizeof key);
        DBT val; memset(&val, 0, sizeof val);
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if (r != 0)
            break;
        uint16_t k;
        assert(key.size == sizeof k);
        memcpy(&k, key.data, key.size);
        assert(k == i);

        uint32_t total_rows = get_total_num_keys(i, ndbs); 
        assert(val.size == total_rows * sizeof (uint32_t));
        uint32_t v[total_rows]; get_data(v, i, ndbs);
        assert(memcmp(val.data, v, val.size) == 0);
    }
    assert(i == nrows);
    r = cursor->c_close(cursor); assert_zero(r);
    r = txn->commit(txn, 0); assert_zero(r);
}

static void
verify_seq(DB_ENV *env, DB *db, uint8_t dbnum, uint8_t ndbs, uint16_t nrows_primary) {
    assert(dbnum > 0);
    assert(dbnum < ndbs);
    uint32_t nrows = get_total_secondary_rows(nrows_primary);
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DBC *cursor = NULL;
    r = db->cursor(db, txn, &cursor, 0); assert_zero(r);
    uint16_t rows_found = 0;
    uint16_t source_i = 0;
    DBT key; memset(&key, 0, sizeof key);
    DBT val; memset(&val, 0, sizeof val);
    for (source_i = 0; source_i < nrows_primary; ++source_i) {
        uint8_t num_keys = get_num_keys(source_i, dbnum);
        for (uint8_t which = 0; which < num_keys; ++which) {
            r = cursor->c_get(cursor, &key, &val, DB_NEXT);
            CKERR(r);
            uint32_t k;
            assert(key.size == sizeof k);
            memcpy(&k, key.data, key.size);
            assert(k == get_key(source_i, dbnum, which));
            assert(val.size == 0);
            rows_found++;
        }
    }
    r = cursor->c_get(cursor, &key, &val, DB_NEXT);
    CKERR2(r, DB_NOTFOUND);
    assert(rows_found == nrows);
    r = cursor->c_close(cursor); assert_zero(r);
    r = txn->commit(txn, 0); assert_zero(r);
}

static void
verify(DB_ENV *env, DB *db[], int ndbs, int nrows) {
    verify_seq_primary(env, db[0], 0, ndbs, nrows);
    for (int dbnum = 1; dbnum < ndbs; dbnum++) 
        verify_seq(env, db[dbnum], dbnum, ndbs, nrows);
}

static void
verify_empty(DB_ENV *env, DB *db) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DBC *cursor = NULL;
    r = db->cursor(db, txn, &cursor, 0); assert_zero(r);
    int i;
    for (i = 0; ; i++) {
        DBT key; memset(&key, 0, sizeof key);
        DBT val; memset(&val, 0, sizeof val);
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if (r != 0)
            break;
    }
    assert_zero(i);
    r = cursor->c_close(cursor); assert_zero(r);
    r = txn->commit(txn, 0); assert_zero(r);
}

static void
verify_del(DB_ENV *env, DB *db[], int ndbs) {
    for (int dbnum = 0; dbnum < ndbs; dbnum++)
        verify_empty(env, db[dbnum]);
}

static void
populate(DB_ENV *env, DB *db[], uint8_t ndbs, uint16_t nrows, bool del) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DBT_ARRAY key_arrays[ndbs];
    DBT_ARRAY val_arrays[ndbs];
    for (uint8_t i = 0; i < ndbs; ++i) {
        toku_dbt_array_init(&key_arrays[i], 1);
        toku_dbt_array_init(&val_arrays[i], 1);
    }
    // populate
    for (uint16_t i = 0; i < nrows; i++) {
        uint32_t total_rows = get_total_num_keys(i, ndbs); 
        uint16_t k = i;
        uint32_t v[total_rows]; get_data(v, i, ndbs);
        DBT pri_key; dbt_init(&pri_key, &k, sizeof k);
        DBT pri_val; dbt_init(&pri_val, &v[0], sizeof v);
        uint32_t flags[ndbs]; memset(flags, 0, sizeof flags);
        if (del) {
            r = env->del_multiple(env, db[0], txn, &pri_key, &pri_val, ndbs, db, key_arrays, flags);
        } else {
            r = env->put_multiple(env, db[0], txn, &pri_key, &pri_val, ndbs, db, key_arrays, val_arrays, flags);
        }
        assert_zero(r);
        for (int dbnum = 0; dbnum < ndbs; dbnum++)
            verify_locked(env, db[dbnum], dbnum, i);
    }
    for (uint8_t i = 0; i < ndbs; ++i) {
        toku_dbt_array_destroy(&key_arrays[i]);
        toku_dbt_array_destroy(&val_arrays[i]);
    }

    r = txn->commit(txn, 0); assert_zero(r);
}

static void
run_test(int ndbs, int nrows) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);

    r = env->set_generate_row_callback_for_put(env, put_callback); assert_zero(r);
    r = env->set_generate_row_callback_for_del(env, del_callback); assert_zero(r);

    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *db[ndbs];
    for (uint8_t dbnum = 0; dbnum < ndbs; dbnum++) {
        r = db_create(&db[dbnum], env, 0); assert_zero(r);

        DBT dbt_dbnum; dbt_init(&dbt_dbnum, &dbnum, sizeof dbnum);

        char dbname[32]; sprintf(dbname, "%d.tdb", dbnum);
        r = db[dbnum]->open(db[dbnum], NULL, dbname, NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
        assert_zero(r);

        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
                { int chk_r = db[dbnum]->change_descriptor(db[dbnum], txn_desc, &dbt_dbnum, 0); CKERR(chk_r); } 
        });
    }

    populate(env, db, ndbs, nrows, false);

    verify(env, db, ndbs, nrows);

    populate(env, db, ndbs, nrows, true);

    verify_del(env, db, ndbs);

    for (int dbnum = 0; dbnum < ndbs; dbnum++) 
        r = db[dbnum]->close(db[dbnum], 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
}

int
test_main(int argc, char * const argv[]) {
    int r;
    int ndbs = 16;
    int nrows = 100;

    // parse_args(argc, argv);
    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }
        if (strcmp(arg, "--ndbs") == 0 && i+1 < argc) {
            ndbs = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--nrows") == 0 && i+1 < argc) {
            nrows = atoi(argv[++i]);
            continue;
        }
    }
    //rows should be divisible by max_rows + 1 (so that we have an equal number of each type and we know the total)
    if (nrows % (max_rows_per_primary+1) != 0) {
        nrows += (max_rows_per_primary+1) - (nrows % (max_rows_per_primary+1));
    }
    assert(ndbs >= 0);
    assert(ndbs < (1<<8) - 1);
    assert(nrows >= 0);
    assert(nrows < (1<<15));  // Leave plenty of room

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    run_test(ndbs, nrows);

    return 0;
}

