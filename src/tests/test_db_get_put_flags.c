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
    u_int32_t db_flags;
    u_int32_t flags;
    int       r_expect;
    int       key;
    int       data;
} PUT_TEST;

typedef struct {
    PUT_TEST  put;
    u_int32_t flags;
    int       r_expect;
    int       key;
    int       data;
} GET_TEST;

enum testtype {NONE=0, TGET=1, TPUT=2, SGET=3, SPUT=4, SPGET=5};

typedef struct {
    enum testtype kind;
    u_int32_t     flags;
    int           r_expect;
    int           key;
    int           data;
} TEST;

typedef struct {
    u_int32_t db_flags;
    TEST      tests[4];
} CPUT_TEST;

typedef struct {
    u_int32_t pdb_flags;
    u_int32_t sdb_flags;
    TEST      tests[4];
} STEST;

DB *dbp;
DB *sdbp;
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

void close_secondary() {
    int r;
    r = sdbp->close(sdbp, 0);                           CKERR(r);
}

void insert_bad_flags(DB* dbp, u_int32_t flags, int r_expect, int keyint, int dataint) {
    DBT key;
    DBT data;
    int r;
    
    dbt_init(&key, &keyint, sizeof(keyint));
    dbt_init(&data,&dataint,sizeof(dataint));
    r = dbp->put(dbp, null_txn, &key, &data, flags);
    CKERR2(r, r_expect);
}

void cinsert_bad_flags(DBC* dbc, u_int32_t flags, int r_expect, int keyint, int dataint) {
    DBT key;
    DBT data;
    int r;

    dbt_init(&key, &keyint, sizeof(keyint));
    dbt_init(&data,&dataint,sizeof(dataint));
    r = dbc->c_put(dbc, &key, &data, flags);
    CKERR2(r, r_expect);
}

void get_bad_flags(DB* dbp, u_int32_t flags, int r_expect, int keyint, int dataint) {
    DBT key;
    DBT data;
    int r;
    
    dbt_init(&key, &keyint, sizeof(keyint));
    dbt_init(&data,&dataint,sizeof(dataint));
    r = dbp->get(dbp, null_txn, &key, &data, flags);
    CKERR2(r, r_expect);
    //Verify things don't change.
    assert(*(int*)key.data == keyint);
    assert(*(int*)data.data == dataint);
}

void cinsert_test(TEST tests[4]) {
    int r;
    int i;
    DBC *dbc;
    
    r = dbp->cursor(dbp, null_txn, &dbc, 0);    CKERR(r);
    
    for (i = 0; i < 4; i++) {
        if (tests[i].kind == NONE) break;
        else if (tests[i].kind == TPUT) {
            cinsert_bad_flags(dbc, tests[i].flags, tests[i].r_expect, tests[i].key, tests[i].data);
        }
        else if (tests[i].kind == TGET) {
            get_bad_flags(dbp, tests[i].flags, tests[i].r_expect, tests[i].key, tests[i].data);
        }
        else assert(0);
    }
    
    r = dbc->c_close(dbc);                      CKERR(r);
}

void stest(TEST tests[4]) {
    int i;
    
    for (i = 0; i < 4; i++) {
        if (tests[i].kind == NONE) break;
        else if (tests[i].kind == SGET) {
            get_bad_flags(sdbp, tests[i].flags, tests[i].r_expect, tests[i].key, tests[i].data);
        }
        else assert(0);
    }
}

#ifdef USE_TDB
#define EINVAL_FOR_TDB_OK_FOR_BDB EINVAL
#else
#define DB_YESOVERWRITE 0   //This is just so test numbers stay the same.
#define EINVAL_FOR_TDB_OK_FOR_BDB 0
#endif


PUT_TEST put_tests[] = {
    {0,                 DB_NODUPDATA,    EINVAL, 0, 0},  //r_expect must change to 0, once implemented.
    {DB_DUP|DB_DUPSORT, DB_NODUPDATA,    EINVAL_FOR_TDB_OK_FOR_BDB, 0, 0},  //r_expect must change to 0, and don't skip with BDB once implemented.
    {0,                 DB_YESOVERWRITE, 0,      0, 0},
    {DB_DUP|DB_DUPSORT, DB_YESOVERWRITE, 0,      0, 0},
    {0,                 DB_NOOVERWRITE,  0,      0, 0},
    {DB_DUP|DB_DUPSORT, DB_NOOVERWRITE,  0,      0, 0},
    {0,                 0,               0,      0, 0},
    {DB_DUP|DB_DUPSORT, 0,               EINVAL_FOR_TDB_OK_FOR_BDB, 0, 0},  //r_expect must be EINVAL for TokuDB since DB_DUPSORT doesn't accept put with flags==0
    {DB_DUP|DB_DUPSORT, 0,               EINVAL_FOR_TDB_OK_FOR_BDB, 0, 0},
};
const int num_put = sizeof(put_tests) / sizeof(put_tests[0]);

CPUT_TEST cput_tests[] = {
    {0,                 {{TPUT, 0,            EINVAL, 0, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}, {NONE, }, }},
    {DB_DUP|DB_DUPSORT, {{TPUT, 0,            EINVAL, 0, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}, {NONE, }, }},
    {0,                 {{TPUT, DB_KEYFIRST,  0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_CURRENT,   0,           0, 2}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}}},
    {DB_DUP|DB_DUPSORT, {{TPUT, DB_KEYFIRST,  0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_CURRENT,   EINVAL,      0, 2}, {TGET, DB_GET_BOTH, 0,           0, 1}}},
    {0,                 {{TPUT, DB_KEYLAST,   0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_CURRENT,   0,           0, 2}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}}},
    {DB_DUP|DB_DUPSORT, {{TPUT, DB_KEYLAST,   0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_CURRENT,   EINVAL,      0, 2}, {TGET, DB_GET_BOTH, 0,           0, 1}}},
    {DB_DUP|DB_DUPSORT, {{TPUT, DB_KEYLAST,   0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_CURRENT,   0,           1, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 1, 1}}},
    {DB_DUP|DB_DUPSORT, {{TPUT, DB_KEYLAST,   0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_CURRENT,   0,           1, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}}},
    {0,                 {{TPUT, DB_CURRENT,   EINVAL, 0, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}, {NONE, }, }},
    {DB_DUP|DB_DUPSORT, {{TPUT, DB_CURRENT,   EINVAL, 0, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}, {NONE, }, }},
    {0,                 {{TPUT, DB_NODUPDATA, EINVAL, 0, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}, {NONE, }, }},
    {DB_DUP|DB_DUPSORT, {{TPUT, DB_NODUPDATA, 0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_NODUPDATA, 0,           0, 2}, {TGET, DB_GET_BOTH, 0,           0, 1}, }},
    {DB_DUP|DB_DUPSORT, {{TPUT, DB_NODUPDATA, 0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_NODUPDATA, 0,           0, 2}, {TGET, DB_GET_BOTH, 0,           0, 2}, }},
    {DB_DUP|DB_DUPSORT, {{TPUT, DB_NODUPDATA, 0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_NODUPDATA, DB_KEYEXIST, 0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, }},
};
const int num_cput = sizeof(cput_tests) / sizeof(cput_tests[0]);

GET_TEST get_tests[] = {
    {{0,                 0,                         0, 0, 0}, DB_GET_BOTH, 0,           0, 0},
    {{0,                 0,                         0, 0, 0}, DB_GET_BOTH, 0,           0, 0},
    {{0,                 0,                         0, 0, 0}, DB_GET_BOTH, DB_NOTFOUND, 0, 1},
    {{0,                 DB_YESOVERWRITE, 0, 0, 0}, DB_GET_BOTH, 0,           0, 0},
    {{0,                 DB_YESOVERWRITE, 0, 0, 0}, DB_GET_BOTH, 0,           0, 0},
    {{0,                 DB_YESOVERWRITE, 0, 0, 0}, DB_GET_BOTH, DB_NOTFOUND, 0, 1},
    {{DB_DUP|DB_DUPSORT, DB_YESOVERWRITE, 0, 0, 0}, DB_GET_BOTH, 0,           0, 0},
    {{DB_DUP|DB_DUPSORT, 0, EINVAL_FOR_TDB_OK_FOR_BDB, 0, 0}, DB_GET_BOTH, IS_TDB ? DB_NOTFOUND : 0, 0, 0},
    {{DB_DUP|DB_DUPSORT, DB_YESOVERWRITE, 0, 0, 0}, DB_GET_BOTH, 0,           0, 0},
    {{DB_DUP|DB_DUPSORT, DB_YESOVERWRITE, 0, 0, 0}, DB_GET_BOTH, DB_NOTFOUND, 0, 1},
    {{0,                 DB_YESOVERWRITE, 0, 0, 0}, DB_RMW,      EINVAL,      0, 0},
    {{DB_DUP|DB_DUPSORT, 0, EINVAL_FOR_TDB_OK_FOR_BDB, 0, 0}, DB_GET_BOTH, DB_NOTFOUND, 0, 1},
    {{0,                 0,                         0, 0, 0}, DB_RMW,      EINVAL,      0, 0},
    {{DB_DUP|DB_DUPSORT, DB_YESOVERWRITE, 0, 0, 0}, DB_RMW,      EINVAL,      0, 0},
};
const int num_get = sizeof(get_tests) / sizeof(get_tests[0]);

STEST stests[] = {
    {0,                 0,                 {{SGET, DB_GET_BOTH, EINVAL, 0, 1}, {NONE, }, }},
};
const int num_stests = sizeof(stests) / sizeof(stests[0]);

int identity_callback(DB *secondary __attribute__((__unused__)), const DBT *key, const DBT *data, DBT *result) {
    memset(result, 0, sizeof(result));
    result->size = key->size;
    result->data = key->data;
    return 0;
}
    
void setup_secondary(u_int32_t flags) {
    int r;

    /* Open/create primary */
    r = db_create(&sdbp, dbenv, 0);                                                     CKERR(r);
    if (flags) {
        r = sdbp->set_flags(dbp, flags);                                                CKERR(r);
    }    
    r = sdbp->open(sdbp, NULL, DIR "/secondary.db", NULL, DB_BTREE, DB_CREATE, 0600);    CKERR(r);
    r = dbp->associate(dbp, NULL, sdbp, identity_callback, 0);                          CKERR(r);
}

int main(int argc, const char *argv[]) {
    int i;
    
    parse_args(argc, argv);
    
    for (i = 0; i < num_put; i++) {
        if (verbose) printf("PutTest [%d]\n", i);
        setup(put_tests[i].db_flags);
        insert_bad_flags(dbp, put_tests[i].flags, put_tests[i].r_expect, put_tests[i].key, put_tests[i].data);
        close_dbs();
    }

    for (i = 0; i < num_get; i++) {
        if (verbose) printf("GetTest [%d]\n", i);
        setup(get_tests[i].put.db_flags);
        insert_bad_flags(dbp, get_tests[i].put.flags, get_tests[i].put.r_expect, get_tests[i].put.key, get_tests[i].put.data);
        get_bad_flags(dbp, get_tests[i].flags, get_tests[i].r_expect, get_tests[i].key, get_tests[i].data);
        close_dbs();
    }

    for (i = 0; i < num_cput; i++) {
        if (verbose) printf("cputTest [%d]\n", i);
        setup(cput_tests[i].db_flags);
        cinsert_test(cput_tests[i].tests);
        close_dbs();
    }
    
    for (i = 0; i < num_stests; i++) {
        if (verbose) printf("stestTest [%d]\n", i);
        setup(stests[i].pdb_flags);
        setup_secondary(stests[i].sdb_flags);
        stest(stests[i].tests);
        close_dbs();
        close_secondary();
    }
    
    

    return 0;
}
