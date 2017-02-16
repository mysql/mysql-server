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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>
#include <ft/txn/xids.h>
#define MAX_NEST MAX_NESTED_TRANSACTIONS


/*********************
 *
 * Purpose of this test is to exercise nested transactions in a basic way:
 * Create MAX nested transactions, inserting a value at each level, verify:
 * 
 * for i = 1 to MAX
 *  - txnid = begin()
 *  - txns[i] = txnid
 *  - insert, query
 *
 * for i = 1 to MAX
 *  - txnid = txns[MAX - i - 1]
 *  - commit or abort(txnid), query
 *
 */

static DB *db;
static DB_ENV *env;

static void
setup_db (void) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    r = db_env_create(&env, 0); CKERR(r);
    r = env->set_default_bt_compare(env, int_dbt_cmp); CKERR(r);
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
    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
}

static void
test_txn_nesting (int depth) {
    int r;
    if (verbose) { fprintf(stderr, "%s (%s):%d [depth = %d]\n", __FILE__, __FUNCTION__, __LINE__, depth); fflush(stderr); }

    DBT key, val, observed_val;
    dbt_init(&observed_val, NULL, 0);
    int i;

    DB_TXN * txns[depth];
    DB_TXN * parent = NULL;

    int vals[depth];

    int mykey = 42;
    dbt_init(&key, &mykey, sizeof mykey);
    

    for (i = 0; i < depth; i++){
	DB_TXN * this_txn;

	if (verbose)
	    printf("Begin txn at level %d\n", i);
	vals[i] = i;
	dbt_init(&val, &vals[i], sizeof i);
	r = env->txn_begin(env, parent, &this_txn, 0);   CKERR(r);
	txns[i] = this_txn;
	parent = this_txn;  // will be parent in next iteration
	r = db->put(db, this_txn, &key, &val, 0);          CKERR(r);

        r = db->get(db, this_txn, &key, &observed_val, 0); CKERR(r);
	assert(int_dbt_cmp(db, &val, &observed_val) == 0);
    }

    int which_val = depth-1;
    for (i = depth-1; i >= 0; i--) {
        //Query, verify the correct value is stored.
        //Close (abort/commit) innermost transaction

        if (verbose)
            printf("Commit txn at level %d\n", i);

        dbt_init(&observed_val, NULL, 0);
        r = db->get(db, txns[i], &key, &observed_val, 0); CKERR(r);
	dbt_init(&val, &vals[which_val], sizeof i);
	assert(int_dbt_cmp(db, &val, &observed_val) == 0);

	if (i % 2) {
	    r = txns[i]->commit(txns[i], DB_TXN_NOSYNC);   CKERR(r);
	    //which_val does not change (it gets promoted)
	}
	else {
	    r = txns[i]->abort(txns[i]); CKERR(r);
	    which_val = i - 1;
	}
        txns[i] = NULL;
    }
    //Query, verify the correct value is stored.
    r = db->get(db, NULL, &key, &observed_val, 0);
    if (which_val == -1) CKERR2(r, DB_NOTFOUND);
    else {
        CKERR(r);
	dbt_init(&val, &vals[which_val], sizeof i);
        assert(int_dbt_cmp(db, &val, &observed_val) == 0);
    }
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    setup_db();
    test_txn_nesting(MAX_NEST);
    close_db();
    return 0;
}
