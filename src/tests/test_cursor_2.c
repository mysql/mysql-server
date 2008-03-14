/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <db.h>

#include "test.h"

void test_cursor() {
    if (verbose) printf("test_cursor\n");

    DB_ENV * env;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.cursor.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_env_create(&env, 0);        assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE|DB_INIT_MPOOL|DB_THREAD, 0777); CKERR(r);
    r = db_create(&db, env, 0); assert(r == 0);
    db->set_errfile(db,stderr); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int i;
    int n = 42;
    for (i=0; i<n; i++) {
        int k = htonl(i);
        int v = htonl(i);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
        assert(r == 0); 
    }

    int ncursors = 2;
    DBC *cursor[ncursors];
    r = db->cursor(db, null_txn, &cursor[0], 0); assert(r == 0);
    r = db->cursor(db, null_txn, &cursor[1], 0); assert(r == 0);

    DBT k0; memset(&k0, 0, sizeof k0);
    DBT v0; memset(&v0, 0, sizeof v0);
    r = cursor[0]->c_get(cursor[0], &k0, &v0, DB_FIRST); assert(r == 0);
    if (verbose) {
	printf("k0:%p:%d\n", k0.data, k0.size);
	printf("v0:%p:%d\n", v0.data, v0.size);
    }

    DBT k1; memset(&k1, 0, sizeof k1);
    DBT v1; memset(&v1, 0, sizeof v1);
    r = cursor[1]->c_get(cursor[1], &k1, &v1, DB_FIRST); assert(r == 0);
    if (verbose) {
	printf("k1:%p:%d\n", k1.data, k1.size);
	printf("v1:%p:%d\n", v1.data, v1.size);
    }

    r = cursor[0]->c_get(cursor[0], &k0, &v0, DB_NEXT); assert(r == 0);
    if (verbose) {
	printf("k0:%p:%d\n", k0.data, k0.size);
	printf("v0:%p:%d\n", v0.data, v0.size);
    }

    r = cursor[0]->c_close(cursor[0]); assert(r == 0);
    r = cursor[1]->c_close(cursor[1]); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);
  
    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);
    
    test_cursor();

    return 0;
}
