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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>
#include <ft/tokuconst.h>
#define MAX_NEST MAX_TRANSACTION_RECORDS
#define MAX_SIZE MAX_TRANSACTION_RECORDS

uint8_t valbufs[MAX_NEST][MAX_SIZE];
DBT vals        [MAX_NEST];
uint8_t keybuf [MAX_SIZE];
DBT key;
DB_TXN   *txns   [MAX_NEST];
DB_TXN   *txn_query;
int which_expected;

static void
fillrandom(uint8_t buf[MAX_SIZE], uint32_t length) {
    assert(length < MAX_SIZE);
    uint32_t i;
    for (i = 0; i < length; i++) {
        buf[i] = random() & 0xFF;
    } 
}

static void
initialize_values (void) {
    int nest_level;
    for (nest_level = 0; nest_level < MAX_NEST; nest_level++) {
        fillrandom(valbufs[nest_level], nest_level);
        dbt_init(&vals[nest_level], &valbufs[nest_level][0], nest_level);
    }
    uint32_t len = random() % MAX_SIZE;
    fillrandom(keybuf, len);
    dbt_init(&key, &keybuf[0], len);
}


/*********************
 *
 * Purpose of this test is to verify nested transactions (support right number of possible values)
for test = 1 to MAX
   create empty db
   for nesting_level = 1 to MAX
     - begin txn
     - insert a value/len unique to this txn
     - query
   abort txn (MAX-test) (test-th innermost) // for test=1 don't abort anything
   commit txn 1 (outermost)                 // for test = MAX don't commit anything
   query                                    // only query that really matters
 */

static DB *db;
static DB_ENV *env;

static void
setup_db (void) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    r = db_env_create(&env, 0); CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_TXN | DB_PRIVATE | DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    CKERR(r);

    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0); CKERR(r);
    }
}


static void
close_db (void) {
    int r;
    r = txn_query->commit(txn_query, 0);
    CKERR(r);
    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
}

static void
verify_val(void) {
    int r;
    DBT observed_val;
    dbt_init(&observed_val, NULL, 0);
    r = db->get(db, txn_query, &key, &observed_val, 0);
    if (which_expected==-1)
        CKERR2(r, DB_NOTFOUND);
    else {
        CKERR(r);
        assert(observed_val.size == vals[which_expected].size);
        assert(memcmp(observed_val.data, vals[which_expected].data, vals[which_expected].size) == 0);
    }
}

static void
initialize_db(void) {
    int r;
    r = env->txn_begin(env, NULL, &txn_query, DB_READ_UNCOMMITTED);
        CKERR(r);
    which_expected = -1;
    verify_val();
    //Put in a 'committed value'
    r = db->put(db, NULL, &key, &vals[0], 0);
        CKERR(r);
    txns[0] = NULL;

    int i;
    which_expected = 0;
    for (i = 1; i < MAX_NEST; i++) {
        r = env->txn_begin(env, txns[i-1], &txns[i], 0);
            CKERR(r);
        verify_val();
        r = db->put(db, txns[i], &key, &vals[i], 0);
            CKERR(r);
        which_expected = i;
        verify_val();
    }
}

static void
test_txn_nested_shortcut (int abort_at_depth) {
    int r;
    if (verbose) { fprintf(stderr, "%s (%s):%d [abortdepth = %d]\n", __FILE__, __FUNCTION__, __LINE__, abort_at_depth); fflush(stderr); }

    setup_db();
    initialize_db();

    which_expected = MAX_NEST-1;
    verify_val();

    assert(abort_at_depth > 0); //Cannot abort 'committed' txn.
    assert(abort_at_depth <= MAX_NEST); //must be in range
    if (abort_at_depth < MAX_NEST) {
        //MAX_NEST means no abort
        DB_TXN *abort_txn = txns[abort_at_depth];
        r = abort_txn->abort(abort_txn);
            CKERR(r);
        which_expected = abort_at_depth - 1;
        verify_val();
    }
    if (abort_at_depth > 1) {
        //abort_at_depth 1 means abort the whole thing (nothing left to commit)
        DB_TXN *commit_txn = txns[1];
        r = commit_txn->commit(commit_txn, DB_TXN_NOSYNC);
            CKERR(r);
        verify_val(); 
    }
    close_db();
}

static void
test_txn_nested_slow (int abort_at_depth) {
    int r;
    if (verbose) { fprintf(stderr, "%s (%s):%d [abortdepth = %d]\n", __FILE__, __FUNCTION__, __LINE__, abort_at_depth); fflush(stderr); }

    setup_db();
    initialize_db();

    which_expected = MAX_NEST-1;
    verify_val();

    assert(abort_at_depth > 0); //Cannot abort 'committed' txn.
    assert(abort_at_depth <= MAX_NEST); //must be in range
    //MAX_NEST means no abort
    int nest;
    for (nest = MAX_NEST - 1; nest >= abort_at_depth; nest--) {
        DB_TXN *abort_txn = txns[nest];
        r = abort_txn->abort(abort_txn);
            CKERR(r);
        which_expected = nest - 1;
        verify_val();
    }
    //which_expected does not change anymore
    for (nest = abort_at_depth-1; nest > 0; nest--) {
        DB_TXN *commit_txn = txns[nest];
        r = commit_txn->commit(commit_txn, DB_TXN_NOSYNC);
            CKERR(r);
        verify_val(); 
    }
    close_db();
}


int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    initialize_values();
    int i;
    for (i = 1; i <= MAX_NEST; i++) {
        test_txn_nested_shortcut(i);
        test_txn_nested_slow(i);
    }
    return 0;
}
