/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#include "test.h"


#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>

#include <unistd.h>
#include <db.h>
#include <errno.h>

// TOKU_TEST_FILENAME is defined in the Makefile

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.already.exists.ft_handle";
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);                                         CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors
    //    r = db->set_flags(db, DB_DUP);                                           CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);    CKERR(r);
    r = db->close(db, 0);                                                    CKERR(r);
    r = db_create(&db, env, 0);                                         CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);    CKERR(r);
    r = db->close(db, 0);                                                    CKERR(r);
    r = db_create(&db, env, 0);                                         CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);            CKERR(r);
    r = db->close(db, 0);                                                    CKERR(r);

    r = db_create(&db, env, 0);                                         CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_EXCL, 0666);
    assert(r == EINVAL);

    r = db->close(db, 0);                                                    CKERR(r);
    r = db_create(&db, env, 0);                                         CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors

    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE | DB_EXCL, 0666);
    assert(r == EEXIST);
    
    r = db->close(db, 0);                                                    CKERR(r);
    r = env->close(env, 0);                                                  CKERR(r);
    return 0;
}
