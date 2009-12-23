/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>


static void
db_put (DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE);
    assert(r == 0);
}

static void
db_del (DB *db, int k) {
    DB_TXN *const null_txn = 0;
    DBT key;
    int r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), DB_DELETE_ANY);
    assert(r == 0);
}

static void
expect_cursor_get (DBC *cursor, int op, int expectr) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), op);
    assert(r == expectr);
}

static int mycmp(const void *a, const void *b) {
    return memcmp(a, b, sizeof (int));
}

static void
test_dupsort_delete (int n) {
    if (verbose) printf("test_dupsort_delete:%d\n", n);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test_dupsort_delete.brt";
    int r;
    int i;

    r = system("rm -rf " ENVDIR); CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->set_flags(db, DB_DUP + DB_DUPSORT); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int values[n];
    for (i=0; i<n; i++)
        values[i] = htonl((i << 16) + (random() & 0xffff));
    int sortvalues[n];
    for (i=0; i<n; i++)
        sortvalues[i] = values[i];
    qsort(sortvalues, n, sizeof sortvalues[0], mycmp);

    for (i=0; i<n; i++) {
        db_put(db, htonl(n), values[i]);
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0); assert(r == 0);
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->set_flags(db, DB_DUP + DB_DUPSORT); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666); assert(r == 0);

    db_del(db, htonl(n));

    for (i=0; i<n; i++) {
        db_put(db, htonl(0), values[i]);
    }

    db_del(db, htonl(0));

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    /* verify all gone */
    expect_cursor_get(cursor, DB_NEXT, DB_NOTFOUND); 

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *argv[]) {
    int i;

    parse_args(argc, argv);
  
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    //   test_dupsort_delete(256); return 0;
    
    /* nodup tests */
    for (i = 1; i <= (1<<16); i *= 2) {
        test_dupsort_delete(i);
    }

    return 0;
}
