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

// verify that del_multiple logs individual delete log entries in the recovery log when
// the sum of the log sizes of the individual deletes.

static int
get_key(int i, int dbnum) {
    return htonl(i + dbnum);
}

static void
get_data(int *v, int i, int ndbs) {
    for (int dbnum = 0; dbnum < ndbs; dbnum++) {
        v[dbnum] = get_key(i, dbnum);
    }
}

static int
del_callback(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, const DBT *src_key, const DBT *src_data) {
    toku_dbt_array_resize(dest_keys, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    (void) dest_db; (void) src_db; (void) dest_keys; (void) src_key; (void) src_data;
    assert(src_db == NULL);

    unsigned int dbnum;
    assert(dest_db->descriptor->dbt.size == sizeof dbnum);
    memcpy(&dbnum, dest_db->descriptor->dbt.data, sizeof dbnum);
    assert(dbnum < src_data->size / sizeof (int));
    int *pri_data = (int *) src_data->data;

    assert(dest_key->flags == 0);
    dest_key->size = sizeof (int);
    dest_key->data = &pri_data[dbnum];
    
    return 0;
}

static void
verify_locked(DB_ENV *env, DB *db, int k) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);
    DBT key; dbt_init(&key, &k, sizeof k);
    r = db->del(db, txn, &key, DB_DELETE_ANY); assert(r == DB_LOCK_NOTGRANTED);
    r = txn->abort(txn); assert_zero(r);
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

static inline int
max(int a, int b) {
    return a < b ? b : a;
}

static void
verify_del_multiple(DB_ENV *env, DB *db[], int ndbs, int nrows) {
    int r;
    DB_TXN *deltxn = NULL;
    r = env->txn_begin(env, NULL, &deltxn, 0); assert_zero(r);
    for (int i = 0; i < nrows; i++) {
        int k = get_key(i, 0);
        DBT pri_key; dbt_init(&pri_key, &k, sizeof k);
        int v[max(ndbs,1024)]; get_data(v, i, ndbs);
        DBT pri_data; dbt_init(&pri_data, &v[0], sizeof v);
        DBT keys[ndbs]; memset(keys, 0, sizeof keys);
        uint32_t flags[ndbs]; memset(flags, 0, sizeof flags);
        r = env_del_multiple_test_no_array(env, NULL, deltxn, &pri_key, &pri_data, ndbs, db, keys, flags); assert_zero(r);
        for (int dbnum = 0; dbnum < ndbs; dbnum++) 
            verify_locked(env, db[dbnum], get_key(i, dbnum));
    }
    r = deltxn->commit(deltxn, 0); assert_zero(r);
    for (int dbnum = 0; dbnum < ndbs; dbnum++) 
        verify_empty(env, db[dbnum]);
}

static void
populate_primary(DB_ENV *env, DB *db, int ndbs, int nrows) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    // populate
    for (int i = 0; i < nrows; i++) {
        int k = get_key(i, 0);
        int v[max(ndbs, 1024)]; memset(v, 0, sizeof v); get_data(v, i, ndbs);
        DBT key; dbt_init(&key, &k, sizeof k);
        DBT val; dbt_init(&val, &v[0], sizeof v);
        r = db->put(db, txn, &key, &val, 0); assert_zero(r);
    }

    r = txn->commit(txn, 0); assert_zero(r);
}

static void
populate_secondary(DB_ENV *env, DB *db, int dbnum, int nrows) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    // populate
    for (int i = 0; i < nrows; i++) {
        int k = get_key(i, dbnum);
        DBT key; dbt_init(&key, &k, sizeof k);
        DBT val; dbt_init(&val, NULL, 0);
        r = db->put(db, txn, &key, &val, 0); assert_zero(r);
    }

    r = txn->commit(txn, 0); assert_zero(r);
}

static void
run_test(int ndbs, int nrows) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);

    r = env->set_generate_row_callback_for_del(env, del_callback); assert_zero(r);

    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *db[ndbs];
    for (int dbnum = 0; dbnum < ndbs; dbnum++) {
        r = db_create(&db[dbnum], env, 0); assert_zero(r);

        char dbname[32]; sprintf(dbname, "%d.tdb", dbnum);
        r = db[dbnum]->open(db[dbnum], NULL, dbname, NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

        DBT dbt_dbnum; dbt_init(&dbt_dbnum, &dbnum, sizeof dbnum);
        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
                { int chk_r = db[dbnum]->change_descriptor(db[dbnum], txn_desc, &dbt_dbnum, 0); CKERR(chk_r); }
        });
    }

    for (int dbnum = 0; dbnum < ndbs; dbnum++) {
        if (dbnum == 0)
            populate_primary(env, db[dbnum], ndbs, nrows);
        else
            populate_secondary(env, db[dbnum], dbnum, nrows);
    }

    verify_del_multiple(env, db, ndbs, nrows);

    for (int dbnum = 0; dbnum < ndbs; dbnum++) 
        r = db[dbnum]->close(db[dbnum], 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
}

int
test_main(int argc, char * const argv[]) {
    int r;
    int ndbs = 2;
    int nrows = 2;

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

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    run_test(ndbs, nrows);

    return 0;
}

