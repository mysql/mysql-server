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

// verify recovery of some update multiple operations

static const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

static int
get_num_new_keys(int i, int dbnum) {
    if (dbnum == 0) return 1;
    if (i & (1<<4)) {
        dbnum++;  // Shift every once in a while.
    }
    return (i + dbnum) % 3;  // 0, 1, or 2
}

static int
get_num_keys(int i, int dbnum) {
    if (dbnum == 0) return 1;
    return (i + dbnum) % 3;  // 0, 1, or 2
}

static int
get_total_secondary_rows(int num_primary) {
    assert(num_primary % 3 == 0);
    return num_primary / 3 * (0 + 1 + 2);
}

static int
get_total_num_keys(int i, int num_dbs) {
    int sum = 0;
    for (int db = 1; db < num_dbs; ++db) {
        sum += get_num_keys(i, db);
    }
    return sum;
}

static int
get_total_num_new_keys(int i, int num_dbs) {
    int sum = 0;
    for (int db = 1; db < num_dbs; ++db) {
        sum += get_num_new_keys(i, db);
    }
    return sum;
}

static int
get_key(int i, int dbnum, int which) {
    assert(i <  INT16_MAX / 2);
    assert(which >= 0);
    assert(which < get_num_keys(i, dbnum));
    assert(which < 4);
    assert(dbnum < 16);
    if (dbnum == 0) {
        assert(which == 0);
        return htonl((2*i) << 16);
    } else {
        return htonl(((2*i+0) << 16) + (dbnum<<8) + (which<<1));
    }
}

static int
get_new_key(int i, int dbnum, int which) {
    assert(which >= 0);
    assert(which < get_num_new_keys(i, dbnum));
    assert(which < 4);
    assert(dbnum < 16);

    if (dbnum == 0) {
        assert(which == 0);
        return htonl((2*i+1) << 16);
    } else if ((i+dbnum+which) & (1<<5)) {
        return htonl(((2*i+0) << 16) + (dbnum<<8) + (which<<1));  // no change from original
    } else {
        return htonl(((2*i+0) << 16) + (dbnum<<8) + (which<<1) + 1);
    }
}

static void
get_data(int *v, int i, int ndbs) {
    int index = 0;
    for (int dbnum = 1; dbnum < ndbs; dbnum++) {
        for (int which = 0; which < get_num_keys(i, dbnum); ++which) {
            v[index++] = get_key(i, dbnum, which);
        }
    }
}

static void
get_new_data(int *v, int i, int ndbs) {
    int index = 0;
    for (int dbnum = 1; dbnum < ndbs; dbnum++) {
        for (int which = 0; which < get_num_new_keys(i, dbnum); ++which) {
            v[index++] = get_new_key(i, dbnum, which);
            if (which > 0) {
                assert(index >= 2);
                assert(memcmp(&v[index-2], &v[index-1], sizeof(v[0])) < 0);
            }
        }
    }
}


static int
put_callback(DB *dest_db, DB *src_db, DBT_ARRAY *dest_key_arrays, DBT_ARRAY *dest_val_arrays, const DBT *src_key, const DBT *src_val) {
    (void)src_val;
    assert(src_db != dest_db);
    assert(src_db);
    int dbnum;
    assert(dest_db->descriptor->dbt.size == sizeof dbnum);
    memcpy(&dbnum, dest_db->descriptor->dbt.data, sizeof dbnum);
    assert(dbnum > 0);

    int pri_key = *(int *) src_key->data;
    int* pri_val = (int*) src_val->data;

    bool is_new = (ntohl(pri_key) >> 16) % 2 == 1;
    int i = (ntohl(pri_key) >> 16) / 2;

    int num_keys = is_new ? get_num_new_keys(i, dbnum) : get_num_keys(i, dbnum);

    toku_dbt_array_resize(dest_key_arrays, num_keys);

    if (dest_val_arrays) {
        toku_dbt_array_resize(dest_val_arrays, num_keys);
    }

    int index = 0;

    for (int idb = 1; idb < dbnum; idb++) {
        index += is_new ? get_num_new_keys(i, idb) : get_num_keys(i, idb);
    }
    assert(src_val->size % sizeof(int) == 0);
    assert((int)src_val->size / 4 >= index + num_keys);

    for (int which = 0; which < num_keys; which++) {
        DBT *dest_key = &dest_key_arrays->dbts[which];
        DBT *dest_val = NULL;

        assert(dest_key->flags == DB_DBT_REALLOC);
        if (dest_key->ulen < sizeof(int)) {
            dest_key->data = toku_xrealloc(dest_key->data, sizeof(int));
            dest_key->ulen = sizeof(int);
        }
        dest_key->size = sizeof(int);
        if (dest_val_arrays) {
            dest_val = &dest_val_arrays->dbts[which];
            assert(dest_val->flags == DB_DBT_REALLOC);
            dest_val->size = 0;
        }
        int new_key = is_new ? get_new_key(i, dbnum, which) : get_key(i, dbnum, which);
        assert(new_key == pri_val[index + which]);
        *(int*)dest_key->data = new_key;
    }
    return 0;
}

static int
del_callback(DB *dest_db, DB *src_db, DBT_ARRAY *dest_key_arrays, const DBT *src_key, const DBT *src_data) {
    return put_callback(dest_db, src_db, dest_key_arrays, NULL, src_key, src_data);
}

static void
update_diagonal(DB_ENV *env, DB_TXN *txn, DB *db[], int ndbs, int nrows) {
    assert(ndbs > 0);
    int r;

    int narrays = 2 * ndbs;
    DBT_ARRAY keys[narrays];
    DBT_ARRAY vals[narrays];
    for (int i = 0; i < narrays; i++) {
        toku_dbt_array_init(&keys[i], 1);
        toku_dbt_array_init(&vals[i], 1);
    }

    for (int i = 0; i < nrows; i++) {

        // update the data i % ndbs col from x to x+1

        int old_k = get_key(i, 0, 0);
        DBT old_key; dbt_init(&old_key, &old_k, sizeof old_k);
        int new_k = get_new_key(i, 0, 0);
        DBT new_key; dbt_init(&new_key, &new_k, sizeof new_k);

        int num_old_keys = get_total_num_keys(i, ndbs);
        int v[num_old_keys]; get_data(v, i, ndbs);
        DBT old_data; dbt_init(&old_data, &v[0], sizeof v);
        
        int num_new_keys = get_total_num_new_keys(i, ndbs);
        int newv[num_new_keys]; get_new_data(newv, i, ndbs);
        DBT new_data; dbt_init(&new_data, &newv[0], sizeof newv);
  
        uint32_t flags_array[ndbs]; memset(flags_array, 0, sizeof(flags_array));

        r = env->update_multiple(env, db[0], txn, &old_key, &old_data, &new_key, &new_data, ndbs, db, flags_array, narrays, keys, narrays, vals);
        assert_zero(r);
    }
    for (int i = 0; i < narrays; i++) {
        toku_dbt_array_destroy(&keys[i]);
        toku_dbt_array_destroy(&vals[i]);
    }

}

static void
populate_primary(DB_ENV *env, DB *db, int ndbs, int nrows) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    // populate
    for (int i = 0; i < nrows; i++) {
        int k = get_key(i, 0, 0);
        int secondary_keys = get_total_num_keys(i, ndbs);
        int v[secondary_keys]; get_data(v, i, ndbs);
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
        for (int which = 0; which < get_num_keys(i, dbnum); which++) {
            int k = get_key(i, dbnum, which);
            DBT key; dbt_init(&key, &k, sizeof k);
            DBT val; dbt_init(&val, NULL, 0);
            r = db->put(db, txn, &key, &val, 0); assert_zero(r);
        }
    }

    r = txn->commit(txn, 0); assert_zero(r);
}

static void
verify_pri_seq(DB_ENV *env, DB *db, int ndbs, int nrows) {
    const int dbnum = 0;
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
        int k;
        int expectk = get_new_key(i, dbnum, 0);
     
        assert(key.size == sizeof k);
        memcpy(&k, key.data, key.size);
        assert(k == expectk);

        int num_keys = get_total_num_new_keys(i, ndbs);
        assert(val.size == num_keys*sizeof(int));
        int v[num_keys]; get_new_data(v, i, ndbs);
        assert(memcmp(val.data, v, val.size) == 0);
    }
    assert(i == nrows); // if (i != nrows) printf("%s:%d %d %d\n", __FUNCTION__, __LINE__, i, nrows); // assert(i == nrows);
    r = cursor->c_close(cursor); assert_zero(r);
    r = txn->commit(txn, 0); assert_zero(r);
}

static void
verify_sec_seq(DB_ENV *env, DB *db, int dbnum, int nrows) {
    assert(dbnum > 0);
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DBC *cursor = NULL;
    r = db->cursor(db, txn, &cursor, 0); assert_zero(r);
    int i;
    int rows_found = 0;

    for (i = 0; ; i++) {
        int num_keys = get_num_new_keys(i, dbnum);
        for (int which = 0; which < num_keys; ++which) {
            DBT key; memset(&key, 0, sizeof key);
            DBT val; memset(&val, 0, sizeof val);
            r = cursor->c_get(cursor, &key, &val, DB_NEXT);
            if (r != 0) {
                CKERR2(r, DB_NOTFOUND);
                goto done;
            }
            rows_found++;
            int k;
            int expectk = get_new_key(i, dbnum, which);
         
            assert(key.size == sizeof k);
            memcpy(&k, key.data, key.size);
            int got_i = (ntohl(k) >> 16) / 2;
            if (got_i < i) {
                // Will fail.  Too many old i's
                assert(k == expectk);
            } else if (got_i > i) {
                // Will fail.  Too few in previous i.
                assert(k == expectk);
            }

            if (k != expectk && which < get_num_keys(i, dbnum) && k == get_key(i, dbnum, which)) {
                // Will fail, never got updated.
                assert(k == expectk);
            }
            assert(k == expectk);
            assert(val.size == 0);
        }
    }
done:
    assert(rows_found == get_total_secondary_rows(nrows));
    r = cursor->c_close(cursor); assert_zero(r);
    r = txn->commit(txn, 0); assert_zero(r);
}

static void
run_test(int ndbs, int nrows) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         assert_zero(r);
    r = env->set_generate_row_callback_for_put(env, put_callback);                      assert_zero(r);
    r = env->set_generate_row_callback_for_del(env, del_callback);                      assert_zero(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      assert_zero(r);

    DB *db[ndbs];
    for (int dbnum = 0; dbnum < ndbs; dbnum++) {
        r = db_create(&db[dbnum], env, 0);                                                        
        assert_zero(r);
        DBT dbt_dbnum; dbt_init(&dbt_dbnum, &dbnum, sizeof dbnum);
        assert_zero(r);
        char dbname[32]; sprintf(dbname, "%d.tdb", dbnum);
        r = db[dbnum]->open(db[dbnum], NULL, dbname, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    
        assert_zero(r);
        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
                { int chk_r = db[dbnum]->change_descriptor(db[dbnum], txn_desc, &dbt_dbnum, 0); CKERR(chk_r); }
        });
    }

    r = env->txn_checkpoint(env, 0, 0, 0);                                              assert_zero(r);

    for (int dbnum = 0; dbnum < ndbs; dbnum++) {
        if (dbnum == 0)
            populate_primary(env, db[dbnum], ndbs, nrows);
        else
            populate_secondary(env, db[dbnum], dbnum, nrows);
    }

    r = env->txn_checkpoint(env, 0, 0, 0);                                              assert_zero(r);

    // update multiple key0
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    update_diagonal(env, txn, db, ndbs, nrows);

    r = txn->commit(txn, 0); assert_zero(r);
    for (int dbnum = 0; dbnum < ndbs; dbnum++) {
        if (dbnum == 0) {
            verify_pri_seq(env, db[0], ndbs, nrows);
        } else {
            verify_sec_seq(env, db[dbnum], dbnum, nrows);
        }
    }

    toku_hard_crash_on_purpose();
}


static void
verify_all(DB_ENV *env, int ndbs, int nrows) {
    int r;
    for (int dbnum = 0; dbnum < ndbs; dbnum++) {
        DB *db = NULL;
        r = db_create(&db, env, 0);                                                        
        assert_zero(r);
        char dbname[32]; sprintf(dbname, "%d.tdb", dbnum);
        r = db->open(db, NULL, dbname, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    
        assert_zero(r);
        if (dbnum == 0) {
            verify_pri_seq(env, db, ndbs, nrows);
        } else {
            verify_sec_seq(env, db, dbnum, nrows);
        }
        r = db->close(db, 0); 
        assert_zero(r);
    }
}

static void
run_recover(int ndbs, int nrows) {
    int r;

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                             assert_zero(r);
    r = env->set_generate_row_callback_for_put(env, put_callback);                          assert_zero(r);
    r = env->set_generate_row_callback_for_del(env, del_callback);                          assert_zero(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);               assert_zero(r);
    verify_all(env, ndbs, nrows);
    r = env->close(env, 0);                                                                 assert_zero(r);
}

static int
usage(void) {
    return 1;
}

int
test_main (int argc, char * const argv[]) {
    bool do_test = false;
    bool do_recover = false;
    int ndbs = 2;
    int nrows = 3*(1<<5)*4;

    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose--;
            if (verbose < 0)
                verbose = 0;
            continue;
        }
        if (strcmp(arg, "--test") == 0) {
            do_test = true;
            continue;
        }
        if (strcmp(arg, "--recover") == 0) {
            do_recover = true;
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
        if (strcmp(arg, "--help") == 0) {
            return usage();
        }
    }
    while (nrows % (3*(1<<5)) != 0) {
        nrows++;
    }
    
    if (do_test)
        run_test(ndbs, nrows);
    if (do_recover)
        run_recover(ndbs, nrows);

    return 0;
}
