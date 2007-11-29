/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <db.h>

// DIR is defined in the Makefile

#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

int main() {
    DB_ENV * env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.db";
    int r;

    system("rm -rf " DIR);

    r=mkdir(DIR, 0777); assert(r==0);

    r=db_env_create(&env, 0);   assert(r==0);
    // Note: without DB_INIT_MPOOL the BDB library will fail on db->open().
    r=env->open(env, DIR, DB_INIT_MPOOL|DB_PRIVATE|DB_CREATE|DB_INIT_LOG, 0777); assert(r==0);

    r = db_create(&db, env, 0);
    CKERR(r);

    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    
    r = db->close(db, 0);
    CKERR(r);

#if 0    
    const char * const fname2 = "test2.db";
    // This sequence segfaults in BDB 4.3.29
    // See what happens if we open a database with a subdb, when the file has only the main db.
    r = db->open(db, null_txn, fname2, 0, DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    r = db->close(db,0);
    CKERR(r);
    r = db->open(db, null_txn, fname2, "main", DB_BTREE, 0, 0666);
    CKERR(r);
    r = db->close(db, 0);
    CKERR(r);
#endif

    r = env->close(env, 0);
    CKERR(r);

    return 0;
}
