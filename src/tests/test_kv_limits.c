/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <db.h>

#include "test.h"

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

void test_key_size_limit(int dup_mode) {
    if (verbose) printf("test_key_size_limit:%d\n", dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test.rand.insert.brt";
    int r;

    system("rm -rf " DIR);
    r=mkdir(DIR, 0777); assert(r==0);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    void *k = 0;
    void *v = 0;
    int lo = 0 , mi = 0, hi = (1<<24);
    int bigest = -1;
    while (lo <= hi) {
        mi = (lo + hi) / 2;
        int ks = mi;
        if (verbose) printf("trying %d %d %d ks=%d\n", lo, mi, hi, ks);
        int vs = ks;
        k = realloc(k, ks);
        memset(k, 0, ks);
        memcpy(k, &ks, sizeof ks);
        v = realloc(v, vs);
        memset(v, 0, vs);
        memcpy(v, &vs, sizeof vs);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, k, ks), dbt_init(&val, v, vs), 0);
        if (r == 0) {
            bigest = mi;
            lo = mi+1;
        } else {
            if (verbose) printf("%d too big\n", ks);
            hi = mi-1;
        }
    }
    free(k);
    free(v);
    assert(bigest > 0);
    if (verbose && bigest >= 0) printf("bigest %d\n", bigest);

    r = db->close(db, 0);
    assert(r == 0);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    test_key_size_limit(0);
    test_key_size_limit(DB_DUP + DB_DUPSORT);

    return 0;
}
