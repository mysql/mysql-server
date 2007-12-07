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

DBT *dbt_init(DBT *dbt, void *data, u_int32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = data;
    dbt->size = size;
    return dbt;
}

DBT *dbt_init_malloc(DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    dbt->flags = DB_DBT_MALLOC;
    return dbt;
}

void db_put(DB *db, int k, int v, u_int32_t put_flags, int rexpect) {
    DBT key, val;
    int r = db->put(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), put_flags);
    if (r != rexpect) {
        printf("Expected %d, got %d\n", rexpect, r);
        if (r != rexpect) errors = 1;
    }
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
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    db_put(db, 0, 0, put_flags, rexpect);
    db_put(db, 0, 0, put_flags, rexpectdupdup);

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

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

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);
  
    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    test_dup_dup(0,                   0,              0,        0);
    test_dup_dup(0,                   DB_NODUPDATA,   EINVAL,   EINVAL);
    test_dup_dup(0,                   DB_NOOVERWRITE, 0,        DB_KEYEXIST);

    test_dup_dup(DB_DUP,              0,              0,        0);
    test_dup_dup(DB_DUP,              DB_NODUPDATA,   EINVAL,   EINVAL);
    test_dup_dup(DB_DUP,              DB_NOOVERWRITE, 0,        DB_KEYEXIST);

    test_dup_dup(DB_DUP | DB_DUPSORT, 0,              0,        DB_KEYEXIST);
    test_dup_dup(DB_DUP | DB_DUPSORT, DB_NODUPDATA,   0,        DB_KEYEXIST);
    test_dup_dup(DB_DUP | DB_DUPSORT, DB_NOOVERWRITE, 0,        DB_KEYEXIST);

    return errors;
}
