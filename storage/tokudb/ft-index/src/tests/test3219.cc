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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// This test, when run under helgrind, should detect the race problem documented in #3219.
// The test:
//   checkpointing runs (in one thread)
//   another thread does an ft lookup.
// We expect to see a lock-acquisition error.


#include "test.h"
#include <pthread.h>


static DB_ENV *env;
static DB *db;

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

static void
lookup(int i, DB_TXN *txn)
// Do a lookup, but don't complain if it's not there.
{
    char hello[30], there[30], expectthere[30];
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(expectthere, sizeof(expectthere), "there%d", i);
    DBT key, val;
    val.data = there;
    val.ulen = sizeof there;
    val.flags = DB_DBT_USERMEM;
    int r=db->get(db, txn,
		  dbt_init(&key, hello, strlen(hello)+1),
		  &val,
		  0);
    if (r==0) {
	assert(val.data==there);
	assert(val.size==strlen(expectthere)+1);
	//printf("Found %s, expected %s\n", there, expectthere);
	assert(strcmp(there, expectthere)==0);
    }
}

#define N_ROWS 1000000
#define N_TXNS 10000
#define N_ROWS_PER_TXN 1

#define INITIAL_SIZE 1000
//#define N_TXNS 10
//#define PER_TXN 10000

static void
setup (void) {
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
	for (int i=0; i<INITIAL_SIZE; i++) insert(random()%N_ROWS, txn);
	r = txn->commit(txn, 0);                                                          CKERR(r);
    }
}


static void
finish (void) {
    int r;
    r = db->close(db, 0);                                                             CKERR(r);
    r = env->close(env, 0);                                                           CKERR(r);
}


volatile int finished = false;

// Thread A performs checkpoints
static void*
start_a (void *arg __attribute__((__unused__))) {
    //r=env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);
    while (!finished) {
	int r;
	r=env->txn_checkpoint(env, 0, 0, 0);                                            CKERR(r);
	sleep(1);
    }
    return NULL;
}

// Thread B performs insertions (eventually they start overwriting the same record).
static void*
start_b (void *arg __attribute__((__unused__))) {
    int r;
    for (int j=0; j<N_TXNS; j++) {
	if (verbose) {
	    printf("."); fflush(stdout);
	    if (j%(N_TXNS/10)==0) printf("\n");
	}
	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
	for (int i=0; i<N_ROWS_PER_TXN; i++) {
	    insert(random()%N_ROWS, txn);
	}
	r = txn->commit(txn, DB_TXN_NOSYNC);                                              CKERR(r);
    }
    finished = true;
    return NULL;
}

// Thread C performs lookups
static void*
start_c (void *arg __attribute__((__unused__))) {
    int r;
    while (!finished) {
	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
	lookup(random()%N_ROWS, txn);
	r = txn->commit(txn, DB_TXN_NOSYNC);                                              CKERR(r);
    }
    return NULL;
}


typedef void *(*pthread_fun)(void*);

static void
run_test (void)
{
    setup();
    pthread_t t[3];
    pthread_fun funs[3] = {start_a, start_b, start_c};
    finished = false;
    for (int i=0; i<3; i++) {
	int r = pthread_create(&t[i], NULL, funs[i], NULL);
	assert(r==0);
    }
    for (int i=0; i<3; i++) {
	void *rv;
	int r = pthread_join(t[i], &rv);
	assert(r==0 && rv==NULL);
    }
    finish();
}

int test_main (int argc, char*const argv[]) {
    parse_args(argc, argv);
    run_test();
    if (verbose) printf("\n");
    return 0;
}
