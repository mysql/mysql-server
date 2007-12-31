/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <db.h>

#include "test.h"

void db_put(DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE);
    assert(r == 0);
}

void db_del(DB *db, int k) {
    DB_TXN *const null_txn = 0;
    DBT key;
    int r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), DB_DELETE_ANY);
    assert(r == 0);
}

void expect_cursor_get(DBC *cursor, int op, int expectr) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), op);
    assert(r == expectr);
}

void test_dupsort_delete(int n) {
    if (verbose) printf("test_dupsort_delete:%d\n", n);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_dupsort_delete.brt";
    int r;
    int i;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, DB_DUP + DB_DUPSORT); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int values[n];
    for (i=0; i<n; i++)
        values[i] = htonl((i << 16) + (random() & 0xffff));
    int sortvalues[n];
    for (i=0; i<n; i++)
        sortvalues[i] = values[i];
    int mycmp(const void *a, const void *b) {
        return memcmp(a, b, sizeof (int));
    }
    qsort(sortvalues, n, sizeof sortvalues[0], mycmp);

    for (i=0; i<n; i++) {
        db_put(db, htonl(n), values[i]);
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0); assert(r == 0);
    r = db_create(&db, null_env, 0); assert(r == 0);
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
}

int main(int argc, const char *argv[]) {
    int i;

    parse_args(argc, argv);
  
    system("rm -rf " DIR);
    mkdir(DIR, 0777);
    //   test_dupsort_delete(256); return 0;
    
    /* nodup tests */
    for (i = 1; i <= (1<<16); i *= 2) {
        test_dupsort_delete(i);
    }

    return 0;
}
