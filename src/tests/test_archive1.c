/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
/* Test log archive. */
#include <db.h>
#include <sys/stat.h>
#include <memory.h>

int
test_main (int argc, char *argv[]) {
    parse_args(argc, argv);
    DB_ENV *env;
    DB *db;
    DB_TXN *txn;
    int r;

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);

    r=db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r=env->set_lg_max(env, 16000); CKERR(r);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=txn->commit(txn, 0);    CKERR(r);

    int i;
    for (i=0; i<400; i++) {
	DBT key,data;
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
        //this test no longer produces a list with any entries for TDB
        //   - txn_checkpoint trims unused logfiles
#if IS_TDB
        assert(list == 0);
#else
        assert(list);
	assert(list[0]);
	if (verbose) printf("file[0]=%s\n", list[0]);
   	toku_free(list);
#endif
    }

    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
    return 0;
}    
