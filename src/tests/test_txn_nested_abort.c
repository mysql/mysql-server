/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <portability.h>
#include <db.h>

#include "test.h"

static int
db_put (DB *db, DB_TXN *txn, int k, int v) {
    DBT key, val;
    return db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_NOOVERWRITE);
}

static char *db_error(int error) {
    static char errorbuf[32];
    switch (error) {
    case DB_NOTFOUND: return "DB_NOTFOUND";
    case DB_LOCK_DEADLOCK: return "DB_LOCK_DEADLOCK"; 
    case DB_LOCK_NOTGRANTED: return "DB_LOCK_NOTGRANTED";
    case DB_KEYEXIST: return "DB_KEYEXIST";
    default:
        sprintf(errorbuf, "%d", error);
        return errorbuf;
    }
}

static void
test_txn_nested(int do_commit) {
    if (verbose) printf("test_txn_nested:%d\n", do_commit);

    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.txn.nested.abort.brt";
    int r;

    /* create the dup database file */
    r = db_env_create(&env, 0);        assert(r == 0);
    env->set_errfile(env, stderr);
    r = env->open(env, ENVDIR, DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK|DB_INIT_LOG |DB_THREAD |DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_create(&db, env, 0); assert(r == 0);
    db->set_errfile(db,stderr); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE+DB_AUTO_COMMIT, 0666); assert(r == 0);
   
    DB_TXN *t1;
    r = env->txn_begin(env, null_txn, &t1, 0); assert(r == 0);
    if (verbose) printf("t1:begin\n");

    DB_TXN *t2;
    r = env->txn_begin(env, t1, &t2, 0); assert(r == 0);
    if (verbose) printf("t2:begin\n");

    r = db_put(db, t2, htonl(1), htonl(1));
    if (verbose) printf("t1:put:%s\n", db_error(r));

    if (do_commit) {
        r = t2->commit(t2, 0); 
        if (verbose) printf("t2:commit:%s\n", db_error(r));
    } else {
        r = t2->abort(t2);
        if (verbose) printf("t2:abort:%s\n", db_error(r));
    }

    r = db->close(db, 0); assert(r == 0);

    r = t1->commit(t1, 0); 
    if (verbose) printf("t1:commit:%s\n", db_error(r));

    r = env->close(env, 0); assert(r == 0);
}


int main(int argc, const char *argv[]) {

    parse_args(argc, argv);
  
    test_txn_nested(0);
    test_txn_nested(1);

    return 0;
}
