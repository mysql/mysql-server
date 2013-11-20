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

// verify stats after a new row inserted into the root
// verify stats after a row overwrite in the root
// verify stats after a row deletion in the root
// verify stats after an update callback inserts row
// verify stats after an update callback overwrites row
// verify ststs after an update callback deletes row

#include <db.h>
#include <unistd.h>
#include <sys/stat.h>

static int 
my_update_callback(DB *db UU(), const DBT *key UU(), const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra) {
    if (old_val != NULL && old_val->size == 42) // special code for delete
        set_val(NULL, set_extra);
    else
        set_val(extra, set_extra);
    return 0;
}

static void 
run_test (void) {
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r = env->set_redzone(env, 0); CKERR(r);
    env->set_update(env, my_update_callback);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    DB *db = NULL;
    r = db_create(&db, env, 0); CKERR(r);
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = txn->commit(txn, 0);    CKERR(r);

    // verify that stats include a new row inserted into the root
    {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1; char val = 1;
        DBT k,v;
        r = db->put(db, txn, dbt_init(&k, &key, sizeof key), dbt_init(&v, &val, sizeof val), 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);
    }

    // verify that stats are updated by row overwrite in the root
    {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1; int val = 2;
        DBT k,v;
        r = db->put(db, txn, dbt_init(&k, &key, sizeof key), dbt_init(&v, &val, sizeof val), 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);
    }

    // verify that stats are updated by row deletion in the root
    {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1;
        DBT k;
        r = db->del(db, txn, dbt_init(&k, &key, sizeof key), 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys <= 1 && s.bt_dsize == 0); // since garbage collection may not occur, the key count may not be updated

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        // garbage collection has happened in db->close, so 
        // the number of keys should be 0
        assert(s.bt_nkeys == 0 && s.bt_dsize == 0);
    }

    // verify update of non-existing key inserts a row
    //
    //
    // NOTE: #5744 was caught by this test below.
    //
    //
    {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1; char val = 1;
        DBT k = { .data = &key, .size = sizeof key };
        DBT e = { .data = &val, .size = sizeof val };
        r = db->update(db, txn, &k, &e, 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);
    }

    // verify update callback overwrites the row
    {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1; int val = 2;
        DBT k = { .data = &key, .size = sizeof key };
        DBT e = { .data = &val, .size = sizeof val };
        r = db->update(db, txn, &k, &e, 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);
    }

    // verify update callback deletes the row
    {
        // insert a new row
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1; char val[42]; memset(val, 0, sizeof val);
        DBT k = { .data = &key, .size = sizeof key };
        DBT e = { .data = &val, .size = sizeof val };
        r = db->update(db, txn, &k, &e, 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys <= 2 && s.bt_dsize == sizeof key + sizeof val);

        // update it again, this should delete the row
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->update(db, txn, &k, &e, 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys <= 2 && s.bt_dsize == 0); // since garbage collection may not occur, the key count may not be updated

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys <= 2 && s.bt_dsize == 0);
    }

    r = db->close(db, 0);     CKERR(r);

    r = env->close(env, 0);   CKERR(r);
}

int
test_main (int argc , char * const argv[]) {
    parse_args(argc, argv);
    run_test();
    return 0;
}
