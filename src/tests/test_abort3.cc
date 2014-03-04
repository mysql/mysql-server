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
/* Try to exercise all the cases for the leafcommands in ft-ops.c
 */


#include <db.h>
#include <sys/stat.h>

static DB_ENV *env;
static DB *db;
static DB_TXN *txn;

static void insert (int i, int j) {
    char hello[30], there[30];
    DBT key,data;
    if (verbose) printf("Insert %d\n", i);
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(there, sizeof(there), "there%d", j);
    int r = db->put(db, txn,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    dbt_init(&data, there, strlen(there)+1),
		    0);
    CKERR(r);
}

static void op_delete (int i) {
    char hello[30];
    DBT key;
    if (verbose) printf("op_delete %d\n", i);
    snprintf(hello, sizeof(hello), "hello%d", i);
    int r = db->del(db, txn,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    DB_DELETE_ANY);
    assert(r==0);
}

static void lookup (int i, int expect, int expectj) {
    char hello[30], there[30];
    DBT key,data;
    snprintf(hello, sizeof(hello), "hello%d", i);
    memset(&data, 0, sizeof(data));
    if (verbose) printf("Looking up %d (expecting %s)\n", i, expect==0 ? "to find" : "not to find");
    int r = db->get(db, txn,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    &data,
		    0);
    assert(expect==r);
    if (expect==0) {
	CKERR(r);
	snprintf(there, sizeof(there), "there%d", expectj);
	assert(data.size==strlen(there)+1);
	assert(strcmp((char*)data.data, there)==0);
    }
}

static void
test_abort3 (void) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);

    r=db_env_create(&env, 0); assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    insert(0, 0);
    r=txn->commit(txn, 0);    assert(r==0);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    op_delete(0);
    op_delete(1);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);    
    lookup(1, DB_NOTFOUND, -1);
    insert(2, 3);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);    
    insert(2, 4);
    insert(2, 5);
    lookup(2, 0, 5);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    lookup(2, 0, 5);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    insert(3, 0);
    r=txn->commit(txn, 0); CKERR(r);
    
    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    insert(3, 1);
    lookup(3, 0, 1);
    r=txn->abort(txn); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);    
    lookup(3, 0, 0);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    insert(4, 0);
    r=txn->commit(txn, 0); CKERR(r);
    
    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    op_delete(4);
    lookup(4, DB_NOTFOUND, -1);
    r=txn->abort(txn); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);    
    lookup(4, 0, 0);
    r=txn->commit(txn, 0); CKERR(r);


    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    insert(5, 0);
    r=txn->commit(txn, 0); CKERR(r);
    
    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    insert(5, 1);
    lookup(5, 0, 1);
    op_delete(5);
    lookup(5, DB_NOTFOUND, -1);
    r=txn->abort(txn); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);    
    lookup(5, 0, 0);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    r=txn->commit(txn, 0); CKERR(r);
    
    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    insert(6, 0);
    lookup(6, 0, 0);
    op_delete(6);
    lookup(6, DB_NOTFOUND, -1);
    r=txn->abort(txn); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);    
    lookup(6, DB_NOTFOUND, -1);
    r=txn->commit(txn, 0); CKERR(r);


    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
}

int
test_main (int argc, char *const argv[]) {
    parse_args(argc, argv);
    test_abort3();
    return 0;
}
