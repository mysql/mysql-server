/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <toku_portability.h>
#include <db.h>
#include "test.h"

// Recreate a mysqld crash by closing and opening a db within a transaction.
// The crash occurs when writing a dirty cachetable pair, so we insert one
// row.
static void
test_txn_close_open_commit (void) {

#ifndef USE_TDB
#if DB_VERSION_MAJOR==4 && DB_VERSION_MINOR==3
    if (verbose) fprintf(stderr, "%s does not work for BDB %d.%d.   Not running\n", __FILE__, DB_VERSION_MAJOR, DB_VERSION_MINOR);
    return;
#else
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    int r;
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_data_dir(env, ENVDIR);
    r = env->set_lg_dir(env, ENVDIR);
    env->set_errfile(env, stdout);
    r = env->open(env, 0, DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    if (r != 0) printf("%s:%d:%d:%s\n", __FILE__, __LINE__, r, db_strerror(r));
    assert(r == 0);

    DB_TXN *txn = 0;
    r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    {
	struct stat statbuf;
	r = stat(ENVDIR "/test.db", &statbuf);
	assert(r==0);
    }

    DBT key, val;
    int k = 1, v = 1;
    r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
    assert(r == 0);

    // Close before commit
    r = db->close(db, 0); assert(r == 0);

    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, txn, "test.db", 0, DB_UNKNOWN, 0, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    r = txn->commit(txn, 0); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);

    r = env->close(env, 0); assert(r == 0);

    {
	struct stat statbuf;
	r = stat(ENVDIR "/test.db", &statbuf);
	assert(r==0);
    }
#endif
#endif
}

int
test_main(int UU(argc), const char UU(*argv[])) {
    test_txn_close_open_commit();
    return 0;
}
