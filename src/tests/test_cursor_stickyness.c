/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>



static void
db_put (DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
    assert(r == 0);
}

static int
cursor_get (DBC *cursor, unsigned int *k, unsigned int *v, int op) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), op);
    if (r == 0) {
        assert(key.size == sizeof *k); memcpy(k, key.data, key.size);
        assert(val.size == sizeof *v); memcpy(v, val.data, val.size);
    }
    if (key.data) toku_free(key.data);
    if (val.data) toku_free(val.data);
    return r;
}

static void
test_cursor_sticky (int n, int dup_mode) {
    if (verbose) printf("test_cursor_sticky:%d %d\n", n, dup_mode);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test_cursor_sticky.brt";
    int r;

    r = system("rm -rf " ENVDIR); assert(r == 0);
    r = toku_os_mkdir(ENVDIR, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int i;
    unsigned int k, v;
    for (i=0; i<n; i++) {
        db_put(db, htonl(i), htonl(i));
    } 

    /* walk the tree */
    DBC *cursor;
    r = db->cursor(db, 0, &cursor, 0); assert(r == 0);
    for (i=0; i<n; i++) {
        r = cursor_get(cursor, &k, &v, DB_NEXT); assert(r == 0);
        assert(k == htonl(i)); assert(v == htonl(i));
    }

    r = cursor_get(cursor, &k, &v, DB_NEXT); assert(r == DB_NOTFOUND);

    r = cursor_get(cursor, &k, &v, DB_CURRENT); assert(r == 0); assert(k == htonl(n-1)); assert(v == htonl(n-1));

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}


int
test_main(int argc, char *argv[]) {
    int i;

    // setvbuf(stdout, NULL, _IONBF, 0);
    parse_args(argc, argv);
  
    for (i=1; i<65537; i *= 2) {
        test_cursor_sticky(i, 0);
    }
    return 0;
}
