/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/* Test for #3522.    Demonstrate that with DB_TRYAGAIN a cursor can stall.
 * Strategy: Create a tree (with relatively small nodes so things happen quickly, and relatively large compared to the cache).
 *  In a single transaction: Delete everything, and then do a DB_FIRST.
 *  Make the test terminate by capturing the calls to pread(). */

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

#ident "Copyright (c) 2011-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include <portability/toku_atomic.h>

static DB_ENV *env;
static DB *db;
const int N = 1000;

const int n_preads_limit = 1000;
long n_preads = 0;

static ssize_t my_pread (int fd, void *buf, size_t count, off_t offset) {
    long n_read_so_far = toku_sync_fetch_and_add(&n_preads, 1);
    if (n_read_so_far > n_preads_limit) {
	if (verbose) fprintf(stderr, "Apparent infinite loop detected\n");
	abort();
    }
    return pread(fd, buf, count, offset);
}

static void
insert(int i, DB_TXN *txn)
{
    char hello[30], there[30];
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(there, sizeof(there), "there%d", i);
    DBT key, val;
    int r=db->put(db, txn,
		  dbt_init(&key, hello, strlen(hello)+1),
		  dbt_init(&val, there, strlen(there)+1),
		  0);
    CKERR(r);
}

static void op_delete (int i, DB_TXN *x) {
    char hello[30];
    DBT key;
    if (verbose>1) printf("op_delete %d\n", i);
    snprintf(hello, sizeof(hello), "hello%d", i);
    int r = db->del(db, x,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    0);
    CKERR(r);
}

static void
setup (void) {
    db_env_set_func_pread(my_pread);
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create(&env, 0);                                                       CKERR(r);
    r = env->set_redzone(env, 0);                                                     CKERR(r);
    r = env->set_cachesize(env, 0, 128*1024, 1);                                      CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_create(&db, env, 0);                                                       CKERR(r);
    r = db->set_pagesize(db, 4096);                                                   CKERR(r);
    {
	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
	r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
	r = txn->commit(txn, 0);                                                          CKERR(r);
    }
    {
	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
	for (int i=0; i<N; i++) insert(i, txn);
	r = txn->commit(txn, 0);                                                          CKERR(r);
    }
}

static void finish (void) {
    int r;
    r = db->close(db, 0);                                                             CKERR(r);
    r = env->close(env, 0);                                                           CKERR(r);
}


int did_nothing = 0;

static int
do_nothing(DBT const *UU(a), DBT  const *UU(b), void *UU(c)) {
    did_nothing++;
    return 0;
}
static void run_del_next (void) {
    DB_TXN *txn;
    DBC *cursor;
    int r;
    r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
    for (int i=0; i<N; i++) op_delete(i, txn);

    r = db->cursor(db, txn, &cursor, 0);                                              CKERR(r);
    if (verbose) printf("read_next\n");
    n_preads = 0;
    r = cursor->c_getf_next(cursor, 0, do_nothing, NULL);                             CKERR2(r, DB_NOTFOUND);
    assert(did_nothing==0);
    if (verbose) printf("n_preads=%ld\n", n_preads);
    r = cursor->c_close(cursor);                                                      CKERR(r);
    r = txn->commit(txn, 0);                                                          CKERR(r);
}

static void run_del_prev (void) {
    DB_TXN *txn;
    DBC *cursor;
    int r;
    r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
    for (int i=0; i<N; i++) op_delete(i, txn);

    r = db->cursor(db, txn, &cursor, 0);                                              CKERR(r);
    if (verbose) printf("read_prev\n");
    n_preads = 0;
    r = cursor->c_getf_prev(cursor, 0, do_nothing, NULL);                             CKERR2(r, DB_NOTFOUND);
    assert(did_nothing==0);
    if (verbose) printf("n_preads=%ld\n", n_preads);
    r = cursor->c_close(cursor);                                                      CKERR(r);
    r = txn->commit(txn, 0);                                                          CKERR(r);
}

static void run_test (void) {
    setup();
    run_del_next();
    finish();

    setup();
    run_del_prev();
    finish();
}
int test_main (int argc, char*const argv[]) {
    parse_args(argc, argv);
    run_test();
    printf("n_preads=%ld\n", n_preads);
    return 0;
}


