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

// test a comparison function that treats certain different-lengthed keys as equal

struct packed_key {
    char type;
    char k[8];
    static packed_key as_int(int v) {
        packed_key k;
        k.type = 0;
        memcpy(k.k, &v, sizeof(int));
        return k;
    }
    static packed_key as_double(double v) {
        packed_key k;
        k.type = 1;
        memcpy(k.k, &v, sizeof(double));
        return k;
    }
    size_t size() const {
        assert(type == 0 || type == 1);
        return type == 0 ? 5 : 9;
    }
};

// the point is that keys can be packed as integers or doubles, but
// we'll treat them both as doubles for the sake of comparison.
// this means a 4 byte number could equal an 8 byte number.
static int packed_key_cmp(DB *UU(db), const DBT *a, const DBT *b) {
    assert(a->size == 5 || a->size == 9);
    assert(b->size == 5 || b->size == 9);
    char *k1 = reinterpret_cast<char *>(a->data);
    char *k2 = reinterpret_cast<char *>(b->data);
    assert(*k1 == 0 || *k1 == 1);
    assert(*k2 == 0 || *k2 == 1);
    double v1 = *k1 == 0 ? static_cast<double>(*reinterpret_cast<int *>(k1 + 1)) :
                           *reinterpret_cast<double *>(k1 + 1);
    double v2 = *k2 == 0 ? static_cast<double>(*reinterpret_cast<int *>(k2 + 1)) :
                           *reinterpret_cast<double *>(k2 + 1);
    if (v1 > v2) {
        return 1;
    } else if (v1 < v2) {
        return -1;
    } else {
        return 0;
    }
}

static int update_callback(DB *UU(db), const DBT *UU(key), const DBT *old_val, const DBT *extra,
                           void (*set_val)(const DBT *new_val, void *setval_extra), void *setval_extra) {
    assert(extra != nullptr);
    assert(old_val != nullptr);
    assert(extra->size == 0 || extra->size == 100);
    assert(old_val->size == 0 || old_val->size == 100);
    if (extra->data == nullptr) {
        set_val(nullptr, setval_extra);
    } else {
        set_val(extra, setval_extra);
    }
    return 0;
}

enum overwrite_method { 
    VIA_UPDATE_OVERWRITE_BROADCAST,
    VIA_UPDATE_DELETE_BROADCAST,
    VIA_UPDATE_OVERWRITE,
    VIA_UPDATE_DELETE,
    VIA_DELETE,
    VIA_INSERT,
    NUM_OVERWRITE_METHODS
};

static void test_keylen_diff(enum overwrite_method method, bool control_test) {
    int r;

    DB_ENV *env;
    r = db_env_create(&env, 0); CKERR(r);
    r = env->set_default_bt_compare(env, packed_key_cmp); CKERR(r);
    env->set_update(env, update_callback); CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL+DB_INIT_TXN, 0); CKERR(r);

    DB *db;
    r = db_create(&db, env, 0); CKERR(r);
    r = db->set_pagesize(db, 16 * 1024); // smaller pages so we get a more lush tree
    r = db->set_readpagesize(db, 1 * 1024); // smaller basements so we get more per leaf
    r = db->open(db, nullptr, "db", nullptr, DB_BTREE, DB_CREATE, 0666); CKERR(r);

    DBT null_dbt, val_dbt;
    char val_buf[100];
    memset(val_buf, 0, sizeof val_buf);
    dbt_init(&val_dbt, &val_buf, sizeof val_buf);
    dbt_init(&null_dbt, nullptr, 0);

    const int num_keys = 1<<11; //256 * 1000;

    for (int i = 0; i < num_keys; i++) {
        // insert it using a 4 byte key ..
        packed_key key = packed_key::as_int(i);

        DBT dbt;
        dbt_init(&dbt, &key, key.size());
        r = db->put(db, nullptr, &dbt, &val_dbt, 0); CKERR(r);
    }

    // overwrite keys randomly, so we induce flushes and get better / realistic coverage
    int *XMALLOC_N(num_keys, shuffled_keys);
    for (int i = 0; i < num_keys; i++) {
        shuffled_keys[i] = i;
    }
    for (int i = num_keys - 1; i >= 1; i--) {
        long rnd = random64() % (i + 1);
        int tmp = shuffled_keys[rnd];
        shuffled_keys[rnd] = shuffled_keys[i];
        shuffled_keys[i] = tmp;
    }

    for (int i = 0; i < num_keys; i++) {
        // for the control test, delete it using the same length key
        //
        // .. otherwise, delete it with an 8 byte key
        packed_key key = control_test ? packed_key::as_int(shuffled_keys[i]) :
                                        packed_key::as_double(shuffled_keys[i]);

        DBT dbt;
        dbt_init(&dbt, &key, key.size());
        DB_TXN *txn;
        env->txn_begin(env, nullptr, &txn, DB_TXN_NOSYNC); CKERR(r);
        switch (method) {
            case VIA_INSERT: {
                r = db->put(db, txn, &dbt, &val_dbt, 0); CKERR(r);
                break;
            }
            case VIA_DELETE: {
                // we purposefully do not pass DB_DELETE_ANY because the hidden query acts as
                // a sanity check for the control test and, overall, gives better code coverage
                r = db->del(db, txn, &dbt, 0); CKERR(r);
                break;
            }
            case VIA_UPDATE_OVERWRITE:
            case VIA_UPDATE_DELETE: {
                r = db->update(db, txn, &dbt, method == VIA_UPDATE_DELETE ? &null_dbt : &val_dbt, 0); CKERR(r);
                break;
            }
            case VIA_UPDATE_OVERWRITE_BROADCAST:
            case VIA_UPDATE_DELETE_BROADCAST: {
                r = db->update_broadcast(db, txn, method == VIA_UPDATE_DELETE_BROADCAST ? &null_dbt : &val_dbt, 0); CKERR(r);
                if (i > 1 ) { // only need to test broadcast twice - one with abort, one without
                    txn->abort(txn); // we opened a txn so we should abort it before exiting
                    goto done;
                }
                break;
            }
            default: {
                assert(false);
            }
        }
        const bool abort = i % 2 == 0;
        if (abort) {
            txn->abort(txn);
        } else {
            txn->commit(txn, 0);
        }
    }

done:
    toku_free(shuffled_keys);

    // optimize before close to ensure that all messages are applied and any potential bugs are exposed
    r = db->optimize(db);
    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    int r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    for (int i = 0; i < NUM_OVERWRITE_METHODS; i++) {
        enum overwrite_method method = static_cast<enum overwrite_method>(i);

        // control test - must pass for the 'real' test below to be interesting
        printf("testing method %d (control)\n", i);
        test_keylen_diff(method, true);

        // real test, actually mixes key lengths
        printf("testing method %d (real)\n", i);
        test_keylen_diff(method, false);
    }

    return 0;
}
