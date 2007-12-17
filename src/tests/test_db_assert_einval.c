/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <string.h>
#include <db.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "test.h"

// DIR is defined in the Makefile

typedef struct {
	int pkey;
	int skey;
} RECORD;

DB *dbp;
DB_TXN *const null_txn = 0;
DB_ENV *dbenv;

void setup(u_int32_t flags) {
    int r;

    system("rm -rf " DIR);
    mkdir(DIR, 0777);
    /* Open/create primary */
    r = db_create(&dbp, dbenv, 0);                                              CKERR(r);
    if (flags) {
        r = dbp->set_flags(dbp, flags);                                       CKERR(r);
    }    
    r = dbp->open(dbp, NULL, DIR "/primary.db", NULL, DB_BTREE, DB_CREATE, 0600);   CKERR(r);
}

void close_dbs() {
    int r;
    r = dbp->close(dbp, 0);                             CKERR(r);
}

void insert_bad_flags(u_int32_t flags, int r_expect, int keyint, int dataint) {
    DBT key;
    DBT data;
    int r;
    
    dbt_init(&key, &keyint, sizeof(keyint));
    dbt_init(&data,&dataint,sizeof(dataint));
    r = dbp->put(dbp, null_txn, &key, &data, flags);
    CKERR2(r, r_expect);
}

void get_bad_flags(u_int32_t flags, int r_expect, int keyint, int dataint) {
    DBT key;
    DBT data;
    int r;
    
    dbt_init(&key, &keyint, sizeof(keyint));
    dbt_init(&data,&dataint,sizeof(dataint));
    r = dbp->get(dbp, null_txn, &key, &data, flags);
    CKERR2(r, r_expect);
}

typedef struct {
    int       skip_bdb;
    u_int32_t db_flags;
    u_int32_t flags;
    int       r_expect;
    int       key;
    int       data;
} PUT_TEST;

typedef struct {
    PUT_TEST put;
    u_int32_t flags;
    int     r_expect;
    int     key;
    int     data;
} GET_TEST;

PUT_TEST put_tests[] = {
    {0, 0,                 DB_NODUPDATA, EINVAL, 0, 0},
    {1, DB_DUP|DB_DUPSORT, DB_NODUPDATA, EINVAL, 0, 0},  //r_expect must change to 0, and don't skip with BDB once implemented.
};
const int num_put = sizeof(put_tests) / sizeof(put_tests[0]);

GET_TEST get_tests[] = {
    {{0, 0,                 0, 0, 0, 0}, DB_GET_BOTH, 0,           0, 0},
    {{0, 0,                 0, 0, 0, 0}, DB_GET_BOTH, 0,           0, 0},
    {{0, 0,                 0, 0, 0, 0}, DB_GET_BOTH, DB_NOTFOUND, 0, 1},
    {{0, DB_DUP|DB_DUPSORT, 0, 0, 0, 0}, DB_GET_BOTH, 0,           0, 0},
    {{0, DB_DUP|DB_DUPSORT, 0, 0, 0, 0}, DB_GET_BOTH, DB_NOTFOUND, 0, 1},
    {{0, 0,                 0, 0, 0, 0}, DB_RMW,      EINVAL,      0, 0},
    {{0, DB_DUP|DB_DUPSORT, 0, 0, 0, 0}, DB_RMW,      EINVAL,      0, 0},
};
const int num_get = sizeof(get_tests) / sizeof(get_tests[0]);

int main(int argc, const char *argv[]) {
    int i;
    
    parse_args(argc, argv);
    
    for (i = 0; i < num_put; i++) {
        if (verbose) printf("PutTest [%d]\n", i);
#ifndef USE_TDB
        if (!put_tests[i].skip_bdb)
#endif
        {
            setup(put_tests[i].db_flags);
            insert_bad_flags(put_tests[i].flags, put_tests[i].r_expect, put_tests[i].key, put_tests[i].data);
            close_dbs();
        }
    }

    for (i = 0; i < num_get; i++) {
        if (verbose) printf("GetTest [%d]\n", i);
#ifndef USE_TDB
        if (!get_tests[i].put.skip_bdb)
#endif
        {
            setup(get_tests[i].put.db_flags);
            insert_bad_flags(get_tests[i].put.flags, get_tests[i].put.r_expect, get_tests[i].put.key, get_tests[i].put.data);
            get_bad_flags(get_tests[i].flags, get_tests[i].r_expect, get_tests[i].key, get_tests[i].data);
            close_dbs();
        }
    }
    return 0;
}
