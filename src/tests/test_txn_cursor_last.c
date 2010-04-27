/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <sys/stat.h>
#include <db.h>


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

/* t1 t2 l1 l2 p1 p2 c1 c2 */
static void
test_txn_cursor_last_1 (int nrows) {
    if (verbose) printf("test_txn_cursor_last_1:%d\n", nrows);

    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.txn.cursor.last.1.brt";

    /* create the dup database file */
    r = db_env_create(&env, 0);        assert(r == 0);
    env->set_errfile(env, stderr);
    r = env->open(env, ENVDIR, DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK|DB_INIT_LOG |DB_THREAD |DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_create(&db, env, 0); assert(r == 0);
    db->set_errfile(db,stderr); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE+DB_AUTO_COMMIT, 0666); assert(r == 0);
    int i;
    for (i=0; i<nrows; i++) {
        int k = htonl(i);
        int v = htonl(i);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
        assert(r == 0); 
    }
   
    DB_TXN *t1;
    r = env->txn_begin(env, null_txn, &t1, 0); assert(r == 0);
    if (verbose) printf("t1:begin\n");

    DBC *c1;
    r = db->cursor(db, t1, &c1, 0); assert(r == 0);

    DB_TXN *t2;
    r = env->txn_begin(env, null_txn, &t2, 0); assert(r == 0);
    if (verbose) printf("t2:begin\n");

    DBC *c2;
    r = db->cursor(db, t2, &c2, 0); assert(r == 0);

    DBT k1; memset(&k1, 0, sizeof k1);
    DBT v1; memset(&v1, 0, sizeof v1);
    r = c1->c_get(c1, &k1, &v1, DB_LAST);
    if (verbose) printf("c1:last:%s\n", db_error(r));

    r = c1->c_close(c1); assert(r == 0);

    DBT k2; memset(&k2, 0, sizeof k2);
    DBT v2; memset(&v2, 0, sizeof v2);
    r = c2->c_get(c2, &k2, &v2, DB_LAST);
    if (verbose) printf("c2:last:%s\n", db_error(r));

    r = c2->c_close(c2); assert(r == 0);

    int r1 = db_put(db, t1, htonl(nrows), htonl(nrows));
    if (verbose) printf("t1:put:%s\n", db_error(r1));

    int r2 = db_put(db, t2, htonl(nrows), htonl(nrows));
    if (verbose) printf("t2:put:%s\n", db_error(r2));

    if (r1 == 0) {
        r = t1->commit(t1, 0); 
        if (verbose) printf("t1:commit:%s\n", db_error(r));
    } else {
        r = t1->abort(t1);
        if (verbose) printf("t1:abort:%s\n", db_error(r));
    }

    if (r2 == 0) {
        r = t2->commit(t2, 0);
        if (verbose) printf("t2:commit:%s\n", db_error(r));
    } else {
        r = t2->abort(t2);
        if (verbose) printf("t2:abort:%s\n", db_error(r));
    }

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

/* t1 t2 l1 p1 l2 c1 p2 c2 */
static void
test_txn_cursor_last_2 (int nrows) {
    if (verbose) printf("test_txn_cursor_last_2:%d\n", nrows);

    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.txn.cursor.last.1.brt";

    /* create the dup database file */
    r = db_env_create(&env, 0);        assert(r == 0);
    env->set_errfile(env, stderr);
    r = env->open(env, ENVDIR, DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK|DB_INIT_LOG|DB_THREAD|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_create(&db, env, 0); assert(r == 0);
    db->set_errfile(db,stderr); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE+DB_AUTO_COMMIT, 0666); assert(r == 0);
    int i;
    for (i=0; i<nrows; i++) {
        int k = htonl(i);
        int v = htonl(i);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
        assert(r == 0); 
    }
   
    DB_TXN *t1;
    r = env->txn_begin(env, null_txn, &t1, 0); assert(r == 0);
    if (verbose) printf("t1:begin\n");

    DBC *c1;
    r = db->cursor(db, t1, &c1, 0); assert(r == 0);

    DB_TXN *t2;
    r = env->txn_begin(env, null_txn, &t2, 0); assert(r == 0);
    if (verbose) printf("t2:begin\n");

    DBC *c2;
    r = db->cursor(db, t2, &c2, 0); assert(r == 0);

    DBT k1; memset(&k1, 0, sizeof k1);
    DBT v1; memset(&v1, 0, sizeof v1);
    r = c1->c_get(c1, &k1, &v1, DB_LAST);
    if (verbose) printf("c1:last:%s\n", db_error(r));

    r = c1->c_close(c1); assert(r == 0);

    int r1 = db_put(db, t1, htonl(nrows), htonl(nrows));
    if (verbose) printf("t1:put:%s\n", db_error(r1));

    DBT k2; memset(&k2, 0, sizeof k2);
    DBT v2; memset(&v2, 0, sizeof v2);
    r = c2->c_get(c2, &k2, &v2, DB_LAST);
    if (verbose) printf("c2:last:%s\n", db_error(r));

    r = c2->c_close(c2); assert(r == 0);

    if (r1 == 0) {
        r = t1->commit(t1, 0); 
        if (verbose) printf("t1:commit:%s\n", db_error(r));
    } else {
        r = t1->abort(t1);
        if (verbose) printf("t1:abort:%s\n", db_error(r));
    }

    int r2 = db_put(db, t2, htonl(nrows), htonl(nrows));
    if (verbose) printf("t2:put:%s\n", db_error(r2));

    if (r2 == 0) {
        r = t2->commit(t2, 0);
        if (verbose) printf("t2:commit:%s\n", db_error(r));
    } else {
        r = t2->abort(t2);
        if (verbose) printf("t2:abort:%s\n", db_error(r));
    }

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *const argv[]) {

    parse_args(argc, argv);
  
    if (IS_TDB) {
	test_txn_cursor_last_1(0);
	test_txn_cursor_last_1(1);
	test_txn_cursor_last_2(0);
	test_txn_cursor_last_2(1);
    }

    return 0;
}
