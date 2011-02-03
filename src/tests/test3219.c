/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: brt.c 27500 2011-01-18 19:34:22Z bperlman $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// This test, when run under helgrind, should detect the race problem documented in #3219.
// The test:
//   checkpointing runs (in one thread)
//   another thread does a brt lookup.
// We expect to see a lock-acquisition error.


#include "test.h"
#include <pthread.h>


DB_ENV *env;
DB *db;

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
		  DB_YESOVERWRITE);
    CKERR(r);
}

static void
lookup(int i, DB_TXN *txn)
// Do a lookup, but don't complain if it's not there.
{
    char hello[30], there[30], expectthere[30];
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(expectthere, sizeof(expectthere), "there%d", i);
    DBT key, val={.data=there, .ulen=sizeof(there), .flags=DB_DBT_USERMEM};
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
#define N_TXNS 1000000
#define N_ROWS_PER_TXN 1

#define INITIAL_SIZE 1000
//#define N_TXNS 10
//#define PER_TXN 10000

static void
setup (void) {
    int r;
    r = system("rm -rf " ENVDIR);                                                     CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create(&env, 0);                                                       CKERR(r);
    r = env->set_redzone(env, 0);                                                     CKERR(r);
    r = env->set_cachesize(env, 0, 128*1024, 1);                                      CKERR(r);
    r = env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
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


volatile int finished = FALSE;

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
	printf("."); fflush(stdout);
	if (j%(N_TXNS/10)==0) printf("\n");
	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
	for (int i=0; i<N_ROWS_PER_TXN; i++) {
	    insert(random()%N_ROWS, txn);
	}
	r = txn->commit(txn, DB_TXN_NOSYNC);                                              CKERR(r);
    }
    finished = TRUE;
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
    finished = FALSE;
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

int test_main (int argc __attribute__((__unused__)), char*const argv[] __attribute__((__unused__))) {
    run_test();
    printf("\n");
    return 0;
}
