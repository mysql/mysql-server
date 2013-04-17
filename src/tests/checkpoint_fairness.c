/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."
#ident "$Id: test_4015.c 37892 2011-12-15 22:34:16Z bkuszmaul $"

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

DB_ENV *env;
DB     *db;
char   *env_dir = ENVDIR;

const int n_threads = 100;
volatile int reader_start_count = 0;

const int W = 10;
volatile int writer_done_count = 0;

static void *start_txns (void *e) {
    int *idp = e;
    int id = *idp;
    int j;
    DBT k={.size=sizeof(id), .data=&id};
    for (j=0; writer_done_count<W; j++) { // terminate the loop when the checkpoint thread has done it's W items.
	DB_TXN *txn;
	CHK(env->txn_begin(env, NULL, &txn, 0));
	CHK(db->put(db, txn, &k, &k, 0));
	CHK(txn->commit(txn, 0));
	if (j==10) (void)__sync_fetch_and_add(&reader_start_count, 1);
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
	CHK(env->txn_checkpoint(env, 0, 0, 0));
	if (verbose) printf("ck\n");
	sched_yield();
	(void)__sync_fetch_and_add(&writer_done_count, 1);
    }
}

int test_main(int argc, char * const argv[]) {
    parse_args(argc, argv);

    // try to starve the checkpoint
    CHK(db_env_create(&env, 0));
#ifdef USE_TDB
    CHK(env->set_redzone(env, 0));
#endif
    {
	const int size = 10+strlen(env_dir);
	char cmd[size];
	snprintf(cmd, size, "rm -rf %s", env_dir);
	system(cmd);
    }
    CHK(toku_os_mkdir(env_dir, S_IRWXU+S_IRWXG+S_IRWXO));

    const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE | DB_RECOVER;
    CHK(env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO));

    CHK(db_create(&db, env, 0));

    CHK(db->open(db, NULL, "db", NULL, DB_BTREE, DB_CREATE|DB_AUTO_COMMIT, 0666));

    pthread_t thds[n_threads];
    int       ids[n_threads];
    for (int i=0; i<n_threads; i++) {
	ids[i]=i;
	CHK(toku_pthread_create(&thds[i], NULL, start_txns, &ids[i]));
    }
    start_checkpoints();

    for (int i=0; i<n_threads; i++) {
	void *retval;
	CHK(toku_pthread_join(thds[i], &retval));
	assert(retval==NULL);
    }
    CHK(db->close(db, 0));

    CHK(env->close(env, 0));

    return 0;
}
