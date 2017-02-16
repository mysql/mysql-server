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
// This test sets the cache size to be small and then inserts enough data
// to make some basement nodes get evicted.  Then sends a broadcast update
// and checks all the data.  If the msns for evicted basement nodes and
// leaf nodes are not managed properly, this test should fail (because the
// broadcast message will not be applied to basement nodes being brought
// back in).

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

const unsigned int NUM_KEYS = (1<<17);
const unsigned int MAGIC_EXTRA = 0x4ac0ffee;

const char original_data[] = "original: ha.rpbkasrkcabkshtabksraghpkars3cbkarpcpktkpbarkca.hpbtkvaekragptknbnsaotbknotbkaontekhba";
const char updated_data[]  = "updated: crkphi30bi8a9hpckbrap.k98a.pkrh3miachpk0[alr3s4nmubrp8.9girhp,bgoekhrl,nurbperk8ochk,bktoe";

static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *old_val, const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                      void *set_extra),
                      void *set_extra) {
    unsigned int *e;
    assert(extra->size == sizeof(*e));
    CAST_FROM_VOIDP(e, extra->data);
    assert(*e == MAGIC_EXTRA);
    assert(old_val->size == sizeof(original_data));
    assert(memcmp(old_val->data, original_data, sizeof(original_data)) == 0);

    {
        DBT newval;
        set_val(dbt_init(&newval, updated_data, sizeof(updated_data)), set_extra);
    }

    return 0;
}

static int
int_cmp(DB *UU(db), const DBT *a, const DBT *b) {
    unsigned int *ap, *bp;
    assert(a->size == sizeof(*ap));
    CAST_FROM_VOIDP(ap, a->data);
    assert(b->size == sizeof(*bp));
    CAST_FROM_VOIDP(bp, b->data);
    return (*ap > *bp) - (*ap < *bp);
}

static void setup (void) {
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    { int chk_r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    env->set_cachesize(env, 0, 10*(1<<20), 1);
    { int chk_r = env->set_default_bt_compare(env, int_cmp); CKERR(chk_r); }
    { int chk_r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

static void cleanup (void) {
    { int chk_r = env->close(env, 0); CKERR(chk_r); }
}

static int do_inserts(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, val;
    unsigned int i;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, original_data, sizeof(original_data));
    for (i = 0; i < NUM_KEYS; ++i) {
        r = db->put(db, txn, keyp, valp, 0); CKERR(r);
    }
    return r;
}

static int do_updates(DB_TXN *txn, DB *db) {
    DBT extra;
    unsigned int e = MAGIC_EXTRA;
    DBT *extrap = dbt_init(&extra, &e, sizeof(e));
    int r = db->update_broadcast(db, txn, extrap, 0); CKERR(r);
    return r;
}

static int do_verify_results(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, val;
    unsigned int i;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, NULL, 0);
    for (i = 0; i < NUM_KEYS; ++i) {
        r = db->get(db, txn, keyp, valp, 0); CKERR(r);
        assert(val.size == sizeof(updated_data));
        assert(memcmp(val.data, updated_data, sizeof(updated_data)) == 0);
    }
    return r;
}

static int run_test(bool shutdown_before_update, bool shutdown_before_verify) {
    setup();

    DB *db;

    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    db->set_pagesize(db, 256*1024);
    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            { int chk_r = db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }

            { int chk_r = do_inserts(txn_1, db); CKERR(chk_r); }
        });

    if (shutdown_before_update) {
        { int chk_r = db->close(db, 0); CKERR(chk_r); }
        { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
        IN_TXN_COMMIT(env, NULL, txn_reopen, 0, {
                { int chk_r = db->open(db, txn_reopen, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
            });
    }

    IN_TXN_COMMIT(env, NULL, txn_2, 0, {
            { int chk_r = do_updates(txn_2, db); CKERR(chk_r); }
        });

    if (shutdown_before_verify) {
        { int chk_r = db->close(db, 0); CKERR(chk_r); }
        { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
        IN_TXN_COMMIT(env, NULL, txn_reopen, 0, {
                { int chk_r = db->open(db, txn_reopen, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
            });
    }

    IN_TXN_COMMIT(env, NULL, txn_3, 0, {
            { int chk_r = do_verify_results(txn_3, db); CKERR(chk_r); }
        });

    { int chk_r = db->close(db, 0); CKERR(chk_r); }

    cleanup();

    return 0;
}

int test_main(int argc, char * const argv[]) {
    parse_args(argc, argv);

    run_test(false, false);
    run_test(false, true);
    run_test(true, false);
    run_test(true, true);

    return 0;
}
