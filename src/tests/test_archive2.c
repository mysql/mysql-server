/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
/* Test log archive. */
#include <db.h>
#include <sys/stat.h>

int
test_main (int argc, char *const argv[]) {
    parse_args(argc, argv);
    DB_ENV *env;
    DB *db, *db2;
    DB_TXN *txn, *txn2;
    DBT key,data;
    int r;

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);

    r=db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r=env->set_lg_max(env, 20000); CKERR(r);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=txn->commit(txn, 0);    CKERR(r);

    r=db_create(&db2, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r=db->open(db2, txn, "foo2.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=txn->commit(txn, 0);    CKERR(r);

    r=env->txn_begin(env, 0, &txn2, 0); CKERR(r);
    r=db->put(db2, txn2, dbt_init(&key, "what", 5), dbt_init(&data, "who", 4), 0);  CKERR(r);

    int i;
    for (i=0; i<100; i++) {
	char hello[30],there[30];
	snprintf(hello, sizeof(hello), "hello%d", i);
	snprintf(there, sizeof(there), "there%d", i);

	r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
	r=db->put(db, txn,
		  dbt_init(&key,  hello, strlen(hello)+1),
		  dbt_init(&data, there, strlen(there)+1),
		  0);
	r=txn->commit(txn, 0);    CKERR(r);
	r=env->txn_checkpoint(env, 0, 0, 0);
    }

    {
	char **list;
	r=env->log_archive(env, &list, 0);
	CKERR(r);
	assert(list==0); // since there is an open txn
    }

    r=txn2->commit(txn2, 0); CKERR(r);

    r=db->close(db, 0); CKERR(r);
    r=db2->close(db2, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
    return 0;
}    
