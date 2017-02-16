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

#include <portability/toku_pthread.h>
#include <portability/toku_atomic.h>

static DB_ENV *env;
static DB *db;
static DB_TXN *txn1, *txn2;
static const int magic_key = 100;
static int callback_calls;
toku_pthread_t thread1;

static void lock_not_granted(DB *_db, uint64_t requesting_txnid,
                             const DBT *left_key, const DBT *right_key,
                             uint64_t blocking_txnid) {
    toku_sync_fetch_and_add(&callback_calls, 1);
    invariant(strcmp(_db->get_dname(_db), db->get_dname(db)) == 0);
    if (requesting_txnid == txn2->id64(txn2)) {
        invariant(blocking_txnid == txn1->id64(txn1));
        invariant(*reinterpret_cast<int *>(left_key->data) == magic_key);
        invariant(*reinterpret_cast<int *>(right_key->data) == magic_key);
    } else {
        invariant(blocking_txnid == txn2->id64(txn2));
        invariant(*reinterpret_cast<int *>(left_key->data) == magic_key + 1);
        invariant(*reinterpret_cast<int *>(right_key->data) == magic_key + 1);
    }
}

static void acquire_lock(DB_TXN *txn, int key) {
    int val = 0;
    DBT k, v;
    dbt_init(&k, &key, sizeof(int));
    dbt_init(&v, &val, sizeof(int));
    (void) db->put(db, txn, &k, &v, 0);
}

struct acquire_lock_extra {
    acquire_lock_extra(DB_TXN *x, int k) :
        txn(x), key(k) {
    }
    DB_TXN *txn;
    int key;
};

static void *acquire_lock_thread(void *arg) {
    acquire_lock_extra *info = reinterpret_cast<acquire_lock_extra *>(arg);
    acquire_lock(info->txn, info->key);
    return NULL;
}

int test_main(int UU(argc), char *const UU(argv[])) {
    int r;
    const int env_flags = DB_INIT_MPOOL | DB_CREATE | DB_THREAD |
        DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN | DB_PRIVATE;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, 0755); CKERR(r);

    r = db_env_create(&env, 0); CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, env_flags, 0755); CKERR(r);
    r = env->set_lock_timeout(env, 1000, nullptr);
    r = env->set_lock_timeout_callback(env, lock_not_granted);

    r = db_create(&db, env, 0); CKERR(r);
    r = db->open(db, NULL, "test", NULL, DB_BTREE, DB_CREATE, 0777); CKERR(r);

    r = env->txn_begin(env, NULL, &txn1, DB_SERIALIZABLE); CKERR(r);
    r = env->txn_begin(env, NULL, &txn2, DB_SERIALIZABLE); CKERR(r);

    // Extremely simple test. Get lock [0, 0] on txn1, then asynchronously
    // attempt to get that lock in txn2. The timouet callback should get called.

    acquire_lock(txn1, magic_key);
    invariant(callback_calls == 0);

    acquire_lock(txn2, magic_key);
    invariant(callback_calls == 1);

    // If we enduce a deadlock, the callback should get called.
    acquire_lock(txn2, magic_key + 1);
    toku_pthread_t thread;
    acquire_lock_extra e(txn1, magic_key + 1);
    r = toku_pthread_create(&thread, NULL, acquire_lock_thread, &e);
    usleep(100000);
    acquire_lock(txn2, magic_key);
    invariant(callback_calls == 2);
    void *v;
    r = toku_pthread_join(thread, &v); CKERR(r);
    invariant(callback_calls == 3);

    // If we set the callback to null, then it shouldn't get called anymore.
    env->set_lock_timeout_callback(env, nullptr);
    acquire_lock(txn2, magic_key);
    invariant(callback_calls == 3);

    r = txn1->commit(txn1, 0); CKERR(r);
    r = txn2->commit(txn2, 0); CKERR(r);

    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
    return 0;
}
