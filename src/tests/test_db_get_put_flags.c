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
    int       skip_bdb;
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


enum testtype {NONE=0, TGET=1, TPUT=2};

typedef struct {
    enum testtype kind;
    u_int32_t     flags;
    int           r_expect;
    int           key;
    int           data;
} TEST;

typedef struct {
    int       skip_bdb;
    u_int32_t db_flags;
    TEST      tests[4];
} CPUT_TEST;

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

void cinsert_bad_flags(DBC* dbc, u_int32_t flags, int r_expect, int keyint, int dataint) {
    DBT key;
    DBT data;
    int r;

    dbt_init(&key, &keyint, sizeof(keyint));
    dbt_init(&data,&dataint,sizeof(dataint));
    r = dbc->c_put(dbc, &key, &data, flags);
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
            get_bad_flags(tests[i].flags, tests[i].r_expect, tests[i].key, tests[i].data);
        }
        else assert(0);
    }
    
    r = dbc->c_close(dbc);                      CKERR(r);
}

#ifndef USE_TDB
#define DB_YESOVERWRITE 0   //This is just so test numbers stay the same.
#endif


PUT_TEST put_tests[] = {
    {0, 0,                 DB_NODUPDATA,    EINVAL, 0, 0},  //r_expect must change to 0, once implemented.
    {1, DB_DUP|DB_DUPSORT, DB_NODUPDATA,    EINVAL, 0, 0},  //r_expect must change to 0, and don't skip with BDB once implemented.
    {1, 0,                 DB_YESOVERWRITE, 0,      0, 0},
    {1, DB_DUP|DB_DUPSORT, DB_YESOVERWRITE, 0,      0, 0},
    {0, 0,                 DB_NOOVERWRITE,  0,      0, 0},
    {0, DB_DUP|DB_DUPSORT, DB_NOOVERWRITE,  0,      0, 0},
    {0, 0,                 0,               0,      0, 0},  //r_expect must change to EINVAL when/if we no longer accept 0 as flags for put
    {0, DB_DUP|DB_DUPSORT, 0,               0,      0, 0},  //r_expect must change to EINVAL when/if we no longer accept 0 as flags for put
};
const int num_put = sizeof(put_tests) / sizeof(put_tests[0]);

CPUT_TEST cput_tests[] = {
    {0, 0,                 {{TPUT, 0,            EINVAL, 0, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}, {NONE, }, }},
    {0, DB_DUP|DB_DUPSORT, {{TPUT, 0,            EINVAL, 0, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}, {NONE, }, }},
    {0, 0,                 {{TPUT, DB_KEYFIRST,  0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_CURRENT,   0,           0, 2}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}}},
    {0, DB_DUP|DB_DUPSORT, {{TPUT, DB_KEYFIRST,  0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_CURRENT,   EINVAL,      0, 2}, {TGET, DB_GET_BOTH, 0,           0, 1}}},
    {0, 0,                 {{TPUT, DB_KEYLAST,   0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_CURRENT,   0,           0, 2}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}}},
    {0, DB_DUP|DB_DUPSORT, {{TPUT, DB_KEYLAST,   0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_CURRENT,   EINVAL,      0, 2}, {TGET, DB_GET_BOTH, 0,           0, 1}}},
    {0, 0,                 {{TPUT, DB_CURRENT,   EINVAL, 0, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}, {NONE, }, }},
    {0, DB_DUP|DB_DUPSORT, {{TPUT, DB_CURRENT,   EINVAL, 0, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}, {NONE, }, }},
    {0, 0,                 {{TPUT, DB_NODUPDATA, EINVAL, 0, 1}, {TGET, DB_GET_BOTH, DB_NOTFOUND, 0, 1}, {NONE, }, }},
    {0, DB_DUP|DB_DUPSORT, {{TPUT, DB_NODUPDATA, 0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_NODUPDATA, 0,           0, 2}, {TGET, DB_GET_BOTH, 0,           0, 1}, }},
    {0, DB_DUP|DB_DUPSORT, {{TPUT, DB_NODUPDATA, 0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_NODUPDATA, 0,           0, 2}, {TGET, DB_GET_BOTH, 0,           0, 2}, }},
    {0, DB_DUP|DB_DUPSORT, {{TPUT, DB_NODUPDATA, 0,      0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, {TPUT, DB_NODUPDATA, DB_KEYEXIST, 0, 1}, {TGET, DB_GET_BOTH, 0,           0, 1}, }},
};
const int num_cput = sizeof(cput_tests) / sizeof(cput_tests[0]);

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

    for (i = 0; i < num_cput; i++) {
        if (verbose) printf("cputTest [%d]\n", i);
#ifndef USE_TDB
        if (!cput_tests[i].skip_bdb)
#endif
        {
            setup(cput_tests[i].db_flags);
            cinsert_test(cput_tests[i].tests);
            close_dbs();
        }
    }

    return 0;
}
