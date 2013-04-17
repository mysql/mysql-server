/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <sys/wait.h>

const int envflags = DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_RECOVER;

const int my_lg_max  = IS_TDB ? 100 : (4096*2);

int test_main (int UU(argc), char UU(*const argv[])) {
    int r;
    pid_t pid;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);                                 CKERR(r);

    const int N = 5;

    if (0==(pid=fork())) {
	DB_ENV *env;
	r = db_env_create(&env, 0);                                                         CKERR(r);
	r = env->set_lg_max(env, my_lg_max);                                                CKERR(r);
	r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
	DB_TXN *txn;
	r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
	DB *db;
	r = db_create(&db, env, 0);                                                         CKERR(r);
	r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);  CKERR(r);
	r = txn->commit(txn, 0);                                                            CKERR(r);

	r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
	r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);
	for (int i=0; i<N; i++) {
	    DBT k,v;
	    r = db->put(db, txn, dbt_init(&k, &i, sizeof(i)), dbt_init(&v, &i, sizeof(i)), 0); CKERR(r);
	    r = env->txn_checkpoint(env, 0, 0, 0);                                             CKERR(r);
	}
	r = txn->commit(txn, 0);                                                            CKERR(r);

	r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
	r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);
	for (int i=0; i<N; i+=2) {
	    DBT k;
	    r = db->del(db, txn, dbt_init(&k, &i, sizeof(i)), 0);                           CKERR(r);
	    r = env->txn_checkpoint(env, 0, 0, 0);                                          CKERR(r);
	}
	r = txn->commit(txn, 0);                                                            CKERR(r);
	exit(0);

    }
    {
	int status;
	pid_t pid2 = wait(&status);
	assert(pid2==pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
    // Now run recovery to see what happens.
    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
    DB *db;
    r = db_create(&db, env, 0);                                                         CKERR(r);
    r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);  CKERR(r);
    for (int i=0; i<N; i++) {
	DBT k;
	DBT v;
        dbt_init(&v, NULL, 0);
	r = db->get(db, txn, dbt_init(&k, &i, sizeof(i)), &v, 0);
	if (i%2==1) {
	    assert(r==0);
	    //printf("Got %d\n", *(int*)v.data);
	} else {
	    assert(r==DB_NOTFOUND);
	}
    }
    r = txn->commit(txn, 0);                                                            CKERR(r);
    r = db->close(db, 0);                                                               CKERR(r);
    r = env->close(env, 0);                                                             CKERR(r);

    //toku_os_recursive_delete(TOKU_TEST_FILENAME);
    
    return 0;
}
