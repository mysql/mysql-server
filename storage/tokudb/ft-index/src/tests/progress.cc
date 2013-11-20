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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#include "test.h"


/*
 - ydb layer test of progress report on commit, abort.
  - test1:
    create two txns
    perform operations (inserts and deletes)
    commit or abort inner txn
    if abort, verify progress callback was called with correct args
    if commit, verify progress callback was not called
    commit or abort outer txn
    verify progress callback was called with correct args

    Note: inner loop ends with commit, so when outer loop completes,
    it should be called for all operations performed by inner loop.

   perform_ops {
      for i = 0 -> 5 {
          for j = 0 -> 1023
              if (j & 0x20) insert
              else op_delete
   }

   verify (n) {
       verify that callback was called n times with correct args
   }

   test1:
    for c0 = 0, 1 {
        for c1 = 0, 1 {
            begin txn0
            perform_ops (txn0)
            begin txn1
            perform ops (tnx1)
            if c1 
                abort txn1
                verify (n)
            else
                commit txn1
                verify (0)
        }
        if c0
             abort txn0
             verify (2n)
        else 
             commit txn0
             verify (2n)
    }


 - test2
  - create empty dictionary
  - begin txn
  - lock empty dictionary (full range lock)
  - abort 
  - verify that callback was called twice, first with stalled-on-checkpoint true, then with stalled-on-checkpoint false


*/


#define DICT_0 "dict_0.db"
static DB_ENV *env = NULL;
static DB_TXN *txn_parent = NULL;
static DB_TXN *txn_child  = NULL;
static DB_TXN *txn_hold_dname_lock  = NULL;
static DB     *db;
static const char *dname = DICT_0;
static DBT key;
static DBT val;


static void start_txn(void);
static void commit_txn(int);
static void open_db(void);
static void close_db(void);
static void insert(void);
static void op_delete(void);
static void

start_env(void) {
    assert(env==NULL);
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    r = db_env_create(&env, 0);
    CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    dname = DICT_0;

    dbt_init(&key, "key", strlen("key")+1);
    dbt_init(&val, "val", strlen("val")+1);

    open_db();
    close_db();
}

static void
end_env(void) {
    int r;
    r=env->close(env, 0);
    CKERR(r);
    env = NULL;
}

static void
start_txn_prevent_dname_lock(void) {
    assert(env!=NULL);
    assert(txn_hold_dname_lock==NULL);
    int r;
    r=env->txn_begin(env, 0, &txn_hold_dname_lock, 0);
    CKERR(r);
    DB *db2;

    r = db_create(&db2, env, 0);
    CKERR(r);

    r=db2->open(db2, txn_hold_dname_lock, dname, 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r = db2->close(db2, 0);
}

static void nopoll(TOKU_TXN_PROGRESS UU(progress), void *UU(extra)) {
    assert(false);
}

static void
commit_txn_prevent_dname_lock(void) {
    assert(env!=NULL);
    assert(txn_hold_dname_lock!=NULL);
    int r;
    r = txn_hold_dname_lock->commit_with_progress(txn_hold_dname_lock, 0, nopoll, NULL);
    CKERR(r);
    txn_hold_dname_lock = NULL;
}

static void
start_txn(void) {
    assert(env!=NULL);
    int r;
    if (!txn_parent) {
        r=env->txn_begin(env, 0, &txn_parent, 0);
    }
    else {
	assert(!txn_child);
        r=env->txn_begin(env, txn_parent, &txn_child, 0);
    }
    CKERR(r);
}

struct progress_expect {
    int      num_calls;
    uint8_t  is_commit_expected;
    uint8_t  stalled_on_checkpoint_expected;
    uint64_t min_entries_total_expected;
    uint64_t last_entries_processed;
};

static void poll(TOKU_TXN_PROGRESS progress, void *extra) {
    struct progress_expect *CAST_FROM_VOIDP(info, extra);
    info->num_calls++;
    assert(progress->is_commit == info->is_commit_expected);
    assert(progress->stalled_on_checkpoint == info->stalled_on_checkpoint_expected);
    assert(progress->entries_total >= info->min_entries_total_expected);
    assert(progress->entries_processed == 1024 + info->last_entries_processed);
    info->last_entries_processed = progress->entries_processed;
}

//expect_number_polls is number of times polling function should be called.
static void
abort_txn(int expect_number_polls) {
    assert(env!=NULL);
    DB_TXN *txn;
    bool child;
    if (txn_child) {
        txn = txn_child;
        child = true;
    }
    else {
        txn = txn_parent;
        child = false;
    }
    assert(txn);
    
    struct progress_expect extra = {
        .num_calls = 0,
        .is_commit_expected = 0,
        .stalled_on_checkpoint_expected = 0,
        .min_entries_total_expected = (uint64_t) expect_number_polls * 1024,
        .last_entries_processed = 0
    };

    int r;
    r=txn->abort_with_progress(txn, poll, &extra);
    CKERR(r);
    assert(extra.num_calls == expect_number_polls);
    if (child)
        txn_child = NULL;
    else
        txn_parent = NULL;
}

static void
commit_txn(int expect_number_polls) {
    assert(env!=NULL);
    DB_TXN *txn;
    bool child;
    if (txn_child) {
        txn = txn_child;
        child = true;
    }
    else {
        txn = txn_parent;
        child = false;
    }
    assert(txn);
    if (child)
        assert(expect_number_polls == 0);
    
    struct progress_expect extra = {
        .num_calls = 0,
        .is_commit_expected = 1,
        .stalled_on_checkpoint_expected = 0,
        .min_entries_total_expected = (uint64_t) expect_number_polls * 1024,
        .last_entries_processed = 0
    };

    int r;
    r=txn->commit_with_progress(txn, 0, poll, &extra);
    CKERR(r);
    assert(extra.num_calls == expect_number_polls);
    if (child)
        txn_child = NULL;
    else
        txn_parent = NULL;
}

static void
open_db(void) {
    assert(env!=NULL);
    assert(db == NULL);

    int r;

    r = db_create(&db, env, 0);
    CKERR(r);

    r=db->open(db, NULL, dname, 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
}

static void
close_db(void) {
    assert(env!=NULL);
    assert(db != NULL);

    int r;
    r = db->close(db, 0);
    CKERR(r);
    db = NULL;
}

static void
insert(void) {
    assert(env!=NULL);
    assert(db!=NULL);
    DB_TXN *txn = txn_child ? txn_child : txn_parent;
    assert(txn);

    int r=db->put(db, txn,
		  &key,
		  &val,
		  0);
    CKERR(r);
}

static void
op_delete(void) {
    assert(env!=NULL);
    assert(db!=NULL);
    DB_TXN *txn = txn_child ? txn_child : txn_parent;
    assert(txn);

    int r=db->del(db, txn,
		  &key,
		  DB_DELETE_ANY);
    CKERR(r);
}

static void
perform_ops(int n) {
    int i;
    int j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < 1024; j++) {
            if (j & 0x20)
                op_delete();
            else
                insert();
        }
    }
}

static void
progress_test_1(int n, int commit) {
    start_env();
    open_db();
    {
        start_txn();
        {
            start_txn();
            perform_ops(n);
            abort_txn(n);
        }
        {
            start_txn();
            perform_ops(n);
            commit_txn(0);
        }
        perform_ops(n);
        if (commit)
            commit_txn(2*n);
        else
            abort_txn(2*n);
    }
    close_db();
    end_env();
}

static void
abort_txn_stall_checkpoint(void) {
    //We have disabled the norollback log fallback optimization.
    //Checkpoint will not stall
    assert(env!=NULL);
    assert(txn_parent);
    assert(!txn_child);
    
    int r;
    r=txn_parent->abort_with_progress(txn_parent, nopoll, NULL);
    CKERR(r);
    txn_parent = NULL;
}

static void
abort_txn_nostall_checkpoint(void) {
    assert(env!=NULL);
    assert(txn_parent);
    assert(!txn_child);
    
    int r;
    r=txn_parent->abort_with_progress(txn_parent, nopoll, NULL);
    CKERR(r);
    txn_parent = NULL;
}


static void
lock(void) {
    assert(env!=NULL);
    assert(db!=NULL);
    assert(txn_parent);
    assert(!txn_child);

    int r=db->pre_acquire_table_lock(db, txn_parent);
    CKERR(r);
}

static void
progress_test_2(void) {
    start_env();
    open_db();
    start_txn();
    start_txn_prevent_dname_lock();
    lock();
    commit_txn_prevent_dname_lock();
    abort_txn_stall_checkpoint();
    close_db();
    end_env();
}

static void
progress_test_3(void) {
    start_env();
    open_db();
    start_txn();
    lock();
    abort_txn_nostall_checkpoint();
    close_db();
    end_env();
}

int
test_main (int argc, char * const argv[])
{
    parse_args(argc, argv);
    int commit;
    for (commit = 0; commit <= 1; commit++) {
        progress_test_1(4, commit);
    }
    progress_test_2();
    progress_test_3();
    return 0;
}
