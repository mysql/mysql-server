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

void cursor_expect(DBC *cursor, int k, int v, int op) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), op);
    assert(r == 0);
    assert(key.size == sizeof k);
    int kk;
    memcpy(&kk, key.data, key.size);
    assert(val.size == sizeof v);
    int vv;
    memcpy(&vv, val.data, val.size);
    if (kk != k || vv != v) printf("expect key %d got %d - %d %d\n", htonl(k), htonl(kk), htonl(v), htonl(vv));
    assert(kk == k);
    assert(vv == v);

    free(key.data);
    free(val.data);
}

void cursor_expect_fail(DBC *cursor, int op, int expectr) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), op);
    assert(r == expectr);
}
 
/* generate a multi-level tree and delete all entries with a cursor
   verify that the pivot flags are toggled (currently by inspection) */

void test_cursor_delete(int dup_mode) {
    if (verbose) printf("test_cursor_delete:%d\n", dup_mode);

    int pagesize = 4096;
    int elementsize = 32;
    int npp = pagesize/elementsize;
    int n = 16*npp; /* build a 2 level tree */

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test.cursor.delete.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, pagesize); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int i;
    for (i=0; i<n; i++) {
        int k = htonl(dup_mode & DB_DUP ? 1 : i);
        int v = htonl(i);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0); assert(r == 0);
    }

    /* verify the sort order with a cursor */
    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    for (i=0; i<n; i++) {
        cursor_expect(cursor, htonl(dup_mode & DB_DUP ? 1 : i), htonl(i), DB_NEXT); 
        
        r = cursor->c_del(cursor, 0); assert(r == 0);
     }

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}

/* insert duplicate duplicates into a sorted duplicate tree */
void test_cursor_delete_dupsort() {
    if (verbose) printf("test_cursor_delete_dupsort\n");

    int pagesize = 4096;
    int elementsize = 32;
    int npp = pagesize/elementsize;
    int n = 16*npp; /* build a 2 level tree */

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test.cursor.delete.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, DB_DUP + DB_DUPSORT); assert(r == 0);
    r = db->set_pagesize(db, pagesize); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int i;
    for (i=0; i<n; i++) {
        int k = htonl(1);
        int v = htonl(1);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
        if (i == 0) 
            assert(r == 0); 
        else 
            assert(r == DB_KEYEXIST);
    }

    /* verify the sort order with a cursor */
    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    cursor_expect(cursor, htonl(1), htonl(1), DB_NEXT); 
        
    r = cursor->c_del(cursor, 0); assert(r == 0);

    cursor_expect_fail(cursor, DB_NEXT, DB_NOTFOUND);

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);
  
    system("rm -rf " DIR);
    mkdir(DIR, 0777);
    
    test_cursor_delete(0);
#if USE_BDB
    test_cursor_delete(DB_DUP);
#endif
    test_cursor_delete(DB_DUP + DB_DUPSORT);
    test_cursor_delete_dupsort();

    return 0;
}
