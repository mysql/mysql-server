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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#include "test.h"


/*
  - ydb layer test of redirection:
   - create two dictionaries, close
   - create txn
   - open dictionary A
   - redirect (using test-only wrapper in ydb)
   - verify now open to dictionary B
   - abort
   - verify now open to dictionary A
*/

/*
 for N = 0 .. n
     for X == 0 .. x
         for Y == 0 .. N+X
            for c == 0 .. 1
                create two dictionaries (iname A,B), close.
                create txn
                Open N DB handles to dictionary A
                redirect from A to B
                open X more DB handles to dictionary B
                close Y DB handles to dictionary B
                if c ==1 commit else abort
*/

#define DICT_0 "dict_0.db"
#define DICT_1 "dict_1.db"
enum {MAX_DBS = 3};
static DB_ENV *env = NULL;
static DB_TXN *txn = NULL;
static DB     *dbs[MAX_DBS];
static int num_open_dbs = 0;
static const char *dname = DICT_0;
static DBT key;


static void start_txn(void);
static void commit_txn(void);
static void open_db(void);
static void close_db(void);
static void insert(int index, int64_t i);
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

    start_txn();
    open_db();
    insert(0, 0);
    dname = DICT_1;
    open_db();
    insert(1, 1);
    close_db();
    close_db();
    commit_txn();

    dname = DICT_0;
}

static void
end_env(void) {
    int r;
    r=env->close(env, 0);
    CKERR(r);
    env = NULL;
}

static void
start_txn(void) {
    assert(env!=NULL);
    assert(txn==NULL);
    int r;
    r=env->txn_begin(env, 0, &txn, 0);
    CKERR(r);
}

static void
abort_txn(void) {
    assert(env!=NULL);
    assert(txn!=NULL);
    int r;
    r=txn->abort(txn);
    CKERR(r);
    txn = NULL;
}

static void
commit_txn(void) {
    assert(env!=NULL);
    assert(txn!=NULL);
    int r;
    r=txn->commit(txn, 0);
    CKERR(r);
    txn = NULL;
}

static void
open_db(void) {
    assert(env!=NULL);
    assert(txn!=NULL);
    assert(num_open_dbs < MAX_DBS);
    assert(dbs[num_open_dbs] == NULL);

    int r;

    r = db_create(&dbs[num_open_dbs], env, 0);
    CKERR(r);

    DB *db = dbs[num_open_dbs];

    r=db->open(db, txn, dname, 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    num_open_dbs++;
}

static void
close_db(void) {
    assert(env!=NULL);
    assert(num_open_dbs > 0);
    assert(dbs[num_open_dbs-1] != NULL);

    num_open_dbs--;
    int r;
    DB *db = dbs[num_open_dbs];
    r = db->close(db, 0);
    CKERR(r);
    dbs[num_open_dbs] = NULL;
}

static void
insert(int idx, int64_t i) {
    assert(env!=NULL);
    assert(txn!=NULL);
    assert(idx>=0);
    assert(idx<num_open_dbs);

    DB *db = dbs[idx];
    DBT val;
    dbt_init(&val, &i, sizeof(i));
    int r=db->put(db, txn,
		  &key,
		  &val,
		  0);
    CKERR(r);
}

//Verify that ALL dbs point to expected dictionary.
static void
verify(int64_t i) {
    assert(env!=NULL);
    assert(txn!=NULL);
    int r;
    int which;
    for (which = 0; which < num_open_dbs; which++) {
        DB *db = dbs[which];
        assert(db);
        DBT val_expected, val_observed;
        dbt_init(&val_expected, &i, sizeof(i));
        dbt_init(&val_observed, NULL, 0);
        r = db->get(db, txn, &key, &val_observed, 0);
        CKERR(r);
        r = int64_dbt_cmp(db, &val_expected, &val_observed);
        assert(r==0);
    }
}

static void
redirect_dictionary(const char *new_dname, int r_expect) {
    assert(env!=NULL);
    assert(txn!=NULL);
    assert(num_open_dbs>0);
    int r;
    DB *db = dbs[0];
    assert(db!=NULL);
    r = toku_test_db_redirect_dictionary(db, new_dname, txn);      // ydb-level wrapper gets iname of new file and redirects
    CKERR2(r, r_expect);
    if (r==0) {
        dname = new_dname;
    }
}

static void
redirect_EINVAL(void) {
    start_env();
    start_txn();
    dname = DICT_0;
    open_db();
    dname = DICT_1;
    open_db();
    redirect_dictionary(DICT_1, EINVAL);
    insert(1, 1);
    redirect_dictionary(DICT_1, EINVAL);
    close_db();
    redirect_dictionary(DICT_1, EINVAL);
    close_db();
    commit_txn();
    end_env();
}

static void
redirect_test(uint8_t num_open_before, uint8_t num_open_after, uint8_t num_close_after, uint8_t commit) {
    int i;
    start_env();
    start_txn();

    assert(num_open_before > 0);

    for (i = 0; i < num_open_before; i++) {
        open_db();
    }
    verify(0);
    redirect_dictionary(DICT_1, 0);
    verify(1);
    for (i = 0; i < num_open_after; i++) {
        open_db();
    }
    verify(1);
    assert(num_close_after <= num_open_before + num_open_after);
    for (i = 0; i < num_close_after; i++) {
        close_db();
    }
    verify(1);
    if (commit) {
        commit_txn();
        start_txn();
        verify(1);
        commit_txn();
        {
            //Close any remaining open dbs.
            int still_open = num_open_dbs;
            assert(still_open == (num_open_before + num_open_after) - num_close_after);
            for (i = 0; i < still_open; i++) {
                close_db();
            }
        }
    }
    else {
        {
            //Close any remaining open dbs.
            int still_open = num_open_dbs;
            assert(still_open == (num_open_before + num_open_after) - num_close_after);
            for (i = 0; i < still_open; i++) {
                close_db();
            }
        }
        abort_txn();
        start_txn();
        verify(0);
        commit_txn();
    }
    end_env();
}


int
test_main (int argc, char *const argv[])
{
    parse_args(argc, argv);
    redirect_EINVAL();
    int num_open_before;  // number of dbs open before redirect
    int num_open_after;   // number of dbs opened after redirect
    int num_close_after;  // number of dbs closed after redirect
    int commit;
    for (num_open_before = 1; num_open_before <= 2; num_open_before++) {
        for (num_open_after = 0; num_open_after <= 1; num_open_after++) {
            for (num_close_after = 0; num_close_after <= num_open_before+num_open_after; num_close_after++) {
                for (commit = 0; commit <= 1; commit++) {
                    redirect_test(num_open_before, num_open_after, num_close_after, commit);
                }
            }
        }
    }
    return 0;
}
