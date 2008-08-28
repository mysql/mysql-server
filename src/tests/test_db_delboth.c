/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <db.h>

#include "test.h"

#if USE_TDB
enum {INFLATE=128};
void db_put(DB *db, int k, int v) {
    DBT key, val;
    static int vv[INFLATE];
    vv[0] = v;
    int r = db->put(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, vv, sizeof vv), DB_YESOVERWRITE);
    CKERR(r);
}

void expect_db_delboth(DB *db, int k, int v, u_int32_t flags, int expectr) {
    DBT key, val;
    static int vv[INFLATE];
    vv[0] = v;
    int r = db->delboth(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, vv, sizeof vv), flags);
    CKERR2(r, expectr);
}

void expect_db_getboth(DB *db, int k, int v, int expectr) {
    DBT key, val;
    static int vv[INFLATE];
    vv[0] = v;
    int r = db->get(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, vv, sizeof vv), DB_GET_BOTH);
    CKERR2(r, expectr);
}

void test_db_delboth(int n, int dup_mode) {
    if (verbose) printf("test_db_delboth:%d %d\n", n, dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = ENVDIR "/" "test.db.delete.brt";
    int r;

    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777); assert(r==0);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    CKERR(r);
    r = db->set_flags(db, dup_mode);
    CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    CKERR(r);

    /* insert n <i, i> pairs */
    int i;
    for (i=0; i<n; i++) {
        db_put(db, htonl(i), htonl(i));
        if (dup_mode) db_put(db, htonl(i), htonl(i+1));
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    CKERR(r);
    r = db_create(&db, null_env, 0);
    CKERR(r);
    r = db->set_flags(db, dup_mode);
    CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    CKERR(r);

    /* insert n <i, i> pairs */
    for (i=0; i<n; i++) {
        db_put(db, htonl(i), htonl(i));
        if (dup_mode) db_put(db, htonl(i), htonl(i+1));
    }

    for (i=0; i<n/2; i++) {
        //Delete something, key not there.
        expect_db_getboth(db, htonl(i-1), htonl(i+2), DB_NOTFOUND); //Sanity Check
        expect_db_delboth(db, htonl(i-1), htonl(i+2), 0,             DB_NOTFOUND);
        expect_db_getboth(db, htonl(i-1), htonl(i+2), DB_NOTFOUND); //Sanity Check
        expect_db_delboth(db, htonl(i-1), htonl(i+2), DB_DELETE_ANY, 0);
        expect_db_getboth(db, htonl(i-1), htonl(i+2), DB_NOTFOUND); //Sanity Check

        //Delete something, key not there.
        expect_db_delboth(db, htonl(i-1), htonl(i+2), 0,             DB_NOTFOUND);
        expect_db_delboth(db, htonl(i-1), htonl(i+2), DB_DELETE_ANY, 0);

        //Delete something, key there, (key,val) not
        expect_db_delboth(db, htonl(i), htonl(i+2), DB_DELETE_ANY, 0);
        expect_db_delboth(db, htonl(i), htonl(i+2), 0,             DB_NOTFOUND);
        expect_db_delboth(db, htonl(i), htonl(i+2), DB_DELETE_ANY, 0);

        //Verify what we put in still exists
        expect_db_getboth(db, htonl(i), htonl(i),   0);
        expect_db_getboth(db, htonl(i), htonl(i+1), dup_mode ? 0 : DB_NOTFOUND);

        expect_db_delboth(db, htonl(i), htonl(i),   0,             0);
        //Now missing.
        expect_db_getboth(db, htonl(i), htonl(i),   DB_NOTFOUND);
        expect_db_delboth(db, htonl(i), htonl(i),   DB_DELETE_ANY, 0);
        //Still missing.
        expect_db_getboth(db, htonl(i), htonl(i),   DB_NOTFOUND);

        //Still there.
        expect_db_getboth(db, htonl(i), htonl(i+1), dup_mode ? 0 : DB_NOTFOUND);

        expect_db_delboth(db, htonl(i), htonl(i+1), DB_DELETE_ANY, 0);
        expect_db_getboth(db, htonl(i), htonl(i+1), DB_NOTFOUND);
        expect_db_delboth(db, htonl(i), htonl(i+1), 0,             DB_NOTFOUND);
        expect_db_getboth(db, htonl(i), htonl(i+1), DB_NOTFOUND);
    }

    //Reverse order of deletes in second half.
    for (i=n/2; i<n; i++) {
        //Delete something, key not there.
        expect_db_getboth(db, htonl(i-1), htonl(i+2), DB_NOTFOUND); //Sanity Check
        expect_db_delboth(db, htonl(i-1), htonl(i+2), 0,             DB_NOTFOUND);
        expect_db_getboth(db, htonl(i-1), htonl(i+2), DB_NOTFOUND); //Sanity Check
        expect_db_delboth(db, htonl(i-1), htonl(i+2), DB_DELETE_ANY, 0);
        expect_db_getboth(db, htonl(i-1), htonl(i+2), DB_NOTFOUND); //Sanity Check

        //Delete something, key there, (key,val) not
        expect_db_delboth(db, htonl(i), htonl(i+2), DB_DELETE_ANY, 0);
        expect_db_delboth(db, htonl(i), htonl(i+2), 0,             DB_NOTFOUND);
        expect_db_delboth(db, htonl(i), htonl(i+2), DB_DELETE_ANY, 0);

        //Verify what we put in still exists
        expect_db_getboth(db, htonl(i), htonl(i),   0);
        expect_db_getboth(db, htonl(i), htonl(i+1), dup_mode ? 0 : DB_NOTFOUND);

        expect_db_delboth(db, htonl(i), htonl(i),   DB_DELETE_ANY, 0);
        //Now missing.
        expect_db_getboth(db, htonl(i), htonl(i),   DB_NOTFOUND);
        expect_db_delboth(db, htonl(i), htonl(i),   0,             DB_NOTFOUND);
        //Still missing.
        expect_db_getboth(db, htonl(i), htonl(i),   DB_NOTFOUND);

        //Still there.
        expect_db_getboth(db, htonl(i), htonl(i+1), dup_mode ? 0 : DB_NOTFOUND);

        expect_db_delboth(db, htonl(i), htonl(i+1), 0,             dup_mode ? 0 : DB_NOTFOUND);
        expect_db_getboth(db, htonl(i), htonl(i+1), DB_NOTFOUND);
        expect_db_delboth(db, htonl(i), htonl(i+1), DB_DELETE_ANY, 0);
        expect_db_getboth(db, htonl(i), htonl(i+1), DB_NOTFOUND);
    }

    r = db->close(db, 0);
    CKERR(r);
}
#endif //USE_TDB

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);


#if USE_TDB
    test_db_delboth(0, 0);

    int i;
    for (i = 1; i <= (1<<10); i *= 2) {
        test_db_delboth(i, 0);
        test_db_delboth(i, DB_DUP|DB_DUPSORT);
    }
#else
    if (verbose) printf("Test %s not applicable to BDB.\n", __FILE__);
#endif

    return 0;
}

