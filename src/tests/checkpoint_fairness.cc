/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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
