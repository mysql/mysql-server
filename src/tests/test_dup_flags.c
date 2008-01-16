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

/* verify that the dup flags are written and read from the database file correctly */ 
void test_dup_flags(int dup_flags) {
    if (verbose) printf("test_dup_flags:%d\n", dup_flags);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_dup_flags.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_flags);
#if USE_TDB
    if (r != 0 && dup_flags == DB_DUP) {
        if (verbose) printf("%s:%d: WARNING: tokudb does not support DB_DUP\n", __FILE__, __LINE__);
        r = db->close(db, 0); assert(r == 0);
        return;
    }
#endif
    assert(r == 0);
    u_int32_t flags; r = db->get_flags(db, &flags); assert(r == 0); assert(flags == dup_flags);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);
    r = db->close(db, 0); assert(r == 0);

    /* verify dup flags match */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
#if USE_BDB
    if (r == 0 && verbose) 
        printf("%s:%d: WARNING:open ok:dup_mode:%d\n", __FILE__, __LINE__, dup_flags);
#else
    assert(flags ? r != 0 : r == 0);
#endif
    r = db->close(db, 0); assert(r == 0);

    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_flags); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666); assert(r == 0);
    r = db->close(db, 0); assert(r == 0);

    /* verify nodesize match */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_flags); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666); assert(r == 0);
    r = db->close(db, 0); assert(r == 0);
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);
  
    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    /* test flags */
    test_dup_flags(0);
    test_dup_flags(DB_DUP);
    test_dup_flags(DB_DUP + DB_DUPSORT);

    return 0;
}
