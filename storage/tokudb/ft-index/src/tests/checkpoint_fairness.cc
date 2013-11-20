/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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

#ident "Copyright (c) 2011-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

// This test fails if the multi_operation_lock prefers readers.  (See #4347).
// But works well if the multi_operation_lock prefers writers (which, since there is typically only one writer, makes it fair).
// What this test does:
//  Starts a bunch of threads (100 seems to work):  Each executes many transactions (and thus obtains the multi_operation_lock during the txn->commit, and until #4346 is changed, holds it through the fsync.  If we fix #4346 then 
//   this test may not be sensitive to the bug.)
//  Meanwhile another thread tries to do W checkpoints.  (W=10 seems to work).
//  The checkpoint thread waits until all the transaction threads have gotten going (waits until each transaction thread has done 10 transactions).
//  The transaction threads get upset if they manage to run for 1000 transactions without the W checkpoints being finished.
//  The theory is that the transaction threads can starve the checkpoint thread by obtaining the multi_operation_lock.
//  But making the multi_operation_lock prefer writers means that the checkpoint gets a chance to run.

#include "test.h"
#include "toku_pthread.h"
#include <portability/toku_atomic.h>

DB_ENV *env;
DB     *db;
const char   *env_dir = TOKU_TEST_FILENAME;

const int n_threads = 100;
volatile int reader_start_count = 0;

const int W = 10;
volatile int writer_done_count = 0;

static void *start_txns (void *e) {
    int *CAST_FROM_VOIDP(idp, e);
    int id = *idp;
    int j;
    DBT k;
    dbt_init(&k, &id, sizeof id);
    for (j=0; writer_done_count<W; j++) { // terminate the loop when the checkpoint thread has done it's W items.
	DB_TXN *txn;
	{ int chk_r = env->txn_begin(env, NULL, &txn, 0); CKERR(chk_r); }
	{ int chk_r = db->put(db, txn, &k, &k, 0); CKERR(chk_r); }
	{ int chk_r = txn->commit(txn, 0); CKERR(chk_r); }
	if (j==10) (void)toku_sync_fetch_and_add(&reader_start_count, 1);
	if (j%1000==999) { printf("."); fflush(stdout); }
	assert(j<1000); // Get upset if we manage to run this many transactions without the checkpoint thread 
    }
    if (verbose) printf("rdone j=%d\n", j);
    return NULL;
}
static void start_checkpoints (void) {
    while (reader_start_count < n_threads) { sched_yield(); }
    for (int i=0; i<W; i++) {
	if (verbose) printf("cks\n");
	{ int chk_r = env->txn_checkpoint(env, 0, 0, 0); CKERR(chk_r); }
	if (verbose) printf("ck\n");
	sched_yield();
	(void)toku_sync_fetch_and_add(&writer_done_count, 1);
    }
}

int test_main(int argc, char * const argv[]) {
    parse_args(argc, argv);

    // try to starve the checkpoint
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
#ifdef USE_TDB
    { int chk_r = env->set_redzone(env, 0); CKERR(chk_r); }
#endif
    {
	const int size = 10+strlen(env_dir);
	char cmd[size];
	snprintf(cmd, size, "rm -rf %s", env_dir);
	int r = system(cmd);
        CKERR(r);
    }
    { int chk_r = toku_os_mkdir(env_dir, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }

    const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE | DB_RECOVER;
    { int chk_r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }

    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }

    { int chk_r = db->open(db, NULL, "db", NULL, DB_BTREE, DB_CREATE|DB_AUTO_COMMIT, 0666); CKERR(chk_r); }

    pthread_t thds[n_threads];
    int       ids[n_threads];
    for (int i=0; i<n_threads; i++) {
	ids[i]=i;
	{ int chk_r = toku_pthread_create(&thds[i], NULL, start_txns, &ids[i]); CKERR(chk_r); }
    }
    start_checkpoints();

    for (int i=0; i<n_threads; i++) {
	void *retval;
	{ int chk_r = toku_pthread_join(thds[i], &retval); CKERR(chk_r); }
	assert(retval==NULL);
    }
    { int chk_r = db->close(db, 0); CKERR(chk_r); }

    { int chk_r = env->close(env, 0); CKERR(chk_r); }

    return 0;
}
