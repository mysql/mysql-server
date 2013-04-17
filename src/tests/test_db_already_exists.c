/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "test.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <toku_portability.h>
#include <db.h>
#include <errno.h>

// ENVDIR is defined in the Makefile

int
test_main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.already.exists.brt";
    int r;

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);
    r=chdir(ENVDIR);       assert(r==0);

    r = db_create(&db, null_env, 0);                                         CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors
    //    r = db->set_flags(db, DB_DUP);                                           CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);    CKERR(r);
    r = db->close(db, 0);                                                    CKERR(r);
    r = db_create(&db, null_env, 0);                                         CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);    CKERR(r);
    r = db->close(db, 0);                                                    CKERR(r);
    r = db_create(&db, null_env, 0);                                         CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);            CKERR(r);
    r = db->close(db, 0);                                                    CKERR(r);

    r = db_create(&db, null_env, 0);                                         CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_EXCL, 0666);
    assert(r == EINVAL);

    r = db->close(db, 0);                                                    CKERR(r);
    r = db_create(&db, null_env, 0);                                         CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors

    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE | DB_EXCL, 0666);
    assert(r == EEXIST);
    
    r = db->close(db, 0);                                                    CKERR(r);
    return 0;
}
