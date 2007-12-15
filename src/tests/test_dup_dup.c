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
#include <errno.h>

#include "test.h"

int errors;

void db_put(DB *db, int k, int v, u_int32_t put_flags, int rexpect) {
    DBT key, val;
    int r = db->put(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), put_flags);
    if (r != rexpect) {
#if USE_TDB
        if (r == EINVAL && put_flags == DB_NODUPDATA) {
            printf("%s:%d:WARNING:tokdub does not support DB_NODUPDATA yet\n", __FILE__, __LINE__);
            return;
        }
#endif
        printf("Expected %d, got %d\n", rexpect, r);
        if (r != rexpect) errors = 1;
    }
}

void test_dup_key(int dup_mode, u_int32_t put_flags, int rexpect, int rexpectdupdup) {
    if (verbose) printf("test_dup_key: %d, %u, %d, %d\n", dup_mode, put_flags, rexpect, rexpectdupdup);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_insert.brt";
    int r;
 
    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode);
#if USE_TDB
    if (r != 0 && dup_mode == DB_DUP) {
        printf("%s:%d:WARNING: tokudb does not support DB_DUP\n", __FILE__, __LINE__);
        r = db->close(db, 0); assert(r == 0);
        return;
    }
#endif
    assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    db_put(db, 0, 0, put_flags, rexpect);
    db_put(db, 0, 1, put_flags, rexpectdupdup);

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    for (;;) {
        DBT key, val;
        r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);
        if (r != 0) break;
        assert(key.size == sizeof (int));
        assert(val.size == sizeof (int));
        int kk, vv;
        memcpy(&kk, key.data, key.size);
        memcpy(&vv, val.data, val.size);
        if (verbose) printf("kk %d vv %d\n", kk, vv);
        free(key.data);
        free(val.data);
    }

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}

void test_dup_dup(int dup_mode, u_int32_t put_flags, int rexpect, int rexpectdupdup) {
    if (verbose) printf("test_dup_dup: %d, %u, %d, %d\n", dup_mode, put_flags, rexpect, rexpectdupdup);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_insert.brt";
    int r;
 
    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode);
#if USE_TDB
    if (r != 0 && dup_mode == DB_DUP) {
        printf("%s:%d:WARNING: tokudb does not support DB_DUP\n", __FILE__, __LINE__);
        r = db->close(db, 0); assert(r == 0);
        return;
    }
#endif
    assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    db_put(db, 0, 0, put_flags, rexpect);
    db_put(db, 0, 0, put_flags, rexpectdupdup);

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    for (;;) {
        DBT key, val;
        r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);
        if (r != 0) break;
        assert(key.size == sizeof (int));
        assert(val.size == sizeof (int));
        int kk, vv;
        memcpy(&kk, key.data, key.size);
        memcpy(&vv, val.data, val.size);
        if (verbose) printf("kk %d vv %d\n", kk, vv);
        free(key.data);
        free(val.data);
    }

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}

void test_put_00_01_01(int dup_mode, u_int32_t put_flags) {
    if (verbose) printf("test_put_00_01_01: %d, %u\n", dup_mode, put_flags);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_insert.brt";
    int r, expectr;
 
    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode);
#if USE_TDB
    if (r != 0 && dup_mode == DB_DUP) {
        printf("%s:%d:WARNING: tokudb does not support DB_DUP\n", __FILE__, __LINE__);
        r = db->close(db, 0); assert(r == 0);
        return;
    }
#endif
    assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    expectr = 0;
    db_put(db, 0, 0, put_flags, expectr);

    expectr = put_flags == DB_NOOVERWRITE ? DB_KEYEXIST : 0;
    db_put(db, 0, 1, put_flags, expectr);

    expectr = (put_flags == DB_NOOVERWRITE || dup_mode & DB_DUPSORT) ? DB_KEYEXIST : 0;
#if USE_TDB
    if (put_flags == DB_YESOVERWRITE) expectr = 0;
#endif
    db_put(db, 0, 1, put_flags, expectr);

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    for (;;) {
        DBT key, val;
        r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);
        if (r != 0) break;
        assert(key.size == sizeof (int));
        assert(val.size == sizeof (int));
        int kk, vv;
        memcpy(&kk, key.data, key.size);
        memcpy(&vv, val.data, val.size);
        if (verbose) printf("kk %d vv %d\n", kk, vv);
        free(key.data);
        free(val.data);
    }

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);
  
    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    test_put_00_01_01(0, 0); 
    test_put_00_01_01(0, DB_NOOVERWRITE); 

    test_put_00_01_01(DB_DUP | DB_DUPSORT, 0); 
    test_put_00_01_01(DB_DUP | DB_DUPSORT, DB_NOOVERWRITE); 
#if USE_TDB
    test_put_00_01_01(DB_DUP | DB_DUPSORT, DB_YESOVERWRITE);
#endif

    /* dup key uniq data */
    test_dup_key(0,                   0,               0,        0);
    test_dup_key(0,                   DB_NODUPDATA,    EINVAL,   EINVAL);
    test_dup_key(0,                   DB_NOOVERWRITE,  0,        DB_KEYEXIST);

    test_dup_key(DB_DUP,              0,               0,        0);
    test_dup_key(DB_DUP,              DB_NODUPDATA,    EINVAL,   EINVAL);
    test_dup_key(DB_DUP,              DB_NOOVERWRITE,  0,        DB_KEYEXIST);

#if USE_TDB
    //    test_dup_key(DB_DUP | DB_DUPSORT, 0,               EINVAL,   EINVAL);
    test_dup_key(DB_DUP | DB_DUPSORT, 0,               0,        0);
    test_dup_key(DB_DUP | DB_DUPSORT, DB_YESOVERWRITE, 0,        0);
#else
    test_dup_key(DB_DUP | DB_DUPSORT, 0,               0,        0);
#endif
    test_dup_key(DB_DUP | DB_DUPSORT, DB_NODUPDATA,    0,        0);
    test_dup_key(DB_DUP | DB_DUPSORT, DB_NOOVERWRITE,  0,        DB_KEYEXIST);

    /* dup key dup data */
    test_dup_dup(0,                   0,               0,        0);
    test_dup_dup(0,                   DB_NODUPDATA,    EINVAL,   EINVAL);
    test_dup_dup(0,                   DB_NOOVERWRITE,  0,        DB_KEYEXIST);

    test_dup_dup(DB_DUP,              0,               0,        0);
    test_dup_dup(DB_DUP,              DB_NODUPDATA,    EINVAL,   EINVAL);
    test_dup_dup(DB_DUP,              DB_NOOVERWRITE,  0,        DB_KEYEXIST);

#if USE_TDB
    //    test_dup_dup(DB_DUP | DB_DUPSORT, 0,               EINVAL,   EINVAL);
    test_dup_dup(DB_DUP | DB_DUPSORT, 0,               0,        DB_KEYEXIST);
    test_dup_dup(DB_DUP | DB_DUPSORT, DB_YESOVERWRITE, 0,        0);
#else
    test_dup_dup(DB_DUP | DB_DUPSORT, 0              , 0,        DB_KEYEXIST);
#endif
    test_dup_dup(DB_DUP | DB_DUPSORT, DB_NODUPDATA,    0,        DB_KEYEXIST);
    test_dup_dup(DB_DUP | DB_DUPSORT, DB_NOOVERWRITE,  0,        DB_KEYEXIST);

    return errors;
}
