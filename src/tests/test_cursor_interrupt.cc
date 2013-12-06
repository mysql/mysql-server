/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
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

#include <db.h>


int num_interrupts_called;
static bool interrupt(void* extra UU()) {
    num_interrupts_called++;
    return false;
}

static bool interrupt_true(void* extra UU()) {
    num_interrupts_called++;
    return true;
}


int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    DB_ENV *env;
    DB *db;
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_INIT_LOG, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    r = db_create(&db, env, 0);
    CKERR(r);
    r = db->set_readpagesize(db, 1024);
    CKERR(r);
    r = db->set_pagesize(db, 1024*10);
    CKERR(r);

    const char * const fname = "test.change_pagesize";
    r = db->open(db, NULL, fname, "main", DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    DB_TXN* txn;
    r = env->txn_begin(env, 0, &txn, 0);
    CKERR(r);
    for (uint64_t i = 0; i < 10000; i++) {
        DBT key, val;
        uint64_t k = i;
        uint64_t v = i;
        dbt_init(&key, &k, sizeof k);
        dbt_init(&val, &v, sizeof v);
        db->put(db, txn, &key, &val, DB_PRELOCKED_WRITE); // adding DB_PRELOCKED_WRITE just to make the test go faster
    }
    r = txn->commit(txn, 0);
    CKERR(r);

    // create a snapshot txn so that when we delete the elements
    // we just inserted, that they do not get garbage collected away
    DB_TXN* snapshot_txn;
    r = env->txn_begin(env, 0, &snapshot_txn, DB_TXN_SNAPSHOT);
    CKERR(r);

    DB_TXN* delete_txn;
    r = env->txn_begin(env, 0, &delete_txn, DB_TXN_SNAPSHOT);
    CKERR(r);

    for (uint64_t i = 0; i < 10000; i++) {
        DBT key;
        uint64_t k = i;
        dbt_init(&key, &k, sizeof k);
        db->del(db, delete_txn, &key, DB_PRELOCKED_WRITE | DB_DELETE_ANY); // adding DB_PRELOCKED_WRITE just to make the test go faster
    }
    r = delete_txn->commit(delete_txn, 0);
    CKERR(r);

    // to make more than one basement node in the dictionary's leaf nodes
    r = env->txn_checkpoint(env, 0, 0, 0);
    CKERR(r);

    // create a txn that should see an empty dictionary
    DB_TXN* test_txn;
    r = env->txn_begin(env, 0, &test_txn, DB_TXN_SNAPSHOT);
    CKERR(r);
    DBC* cursor = NULL;
    r = db->cursor(db, test_txn, &cursor, 0);
    cursor->c_set_check_interrupt_callback(cursor, interrupt, NULL);
    DBT key, val;
    r = cursor->c_get(cursor, &key, &val, DB_NEXT);
    CKERR2(r, DB_NOTFOUND);
    assert(num_interrupts_called > 1);
    num_interrupts_called = 0;
    cursor->c_set_check_interrupt_callback(cursor, interrupt_true, NULL);
    r = cursor->c_get(cursor, &key, &val, DB_NEXT);
    CKERR2(r, TOKUDB_INTERRUPTED);
    assert(num_interrupts_called == 1);

    r = cursor->c_close(cursor);
    CKERR(r);    
    r = test_txn->commit(test_txn, 0);
    CKERR(r);

    
    r = snapshot_txn->commit(snapshot_txn, 0);
    CKERR(r);


    r = db->close(db, 0);
    CKERR(r);

    r = env->close(env, 0);
    assert(r == 0);

    return 0;
}
