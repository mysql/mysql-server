/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#include "test.h"

/* Simple test of logging.  Can I start a TokuDB with logging enabled? */

#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <db.h>


// TOKU_TEST_FILENAME is defined in the Makefile

DB_ENV *env;
DB *db;

int
test_main (int UU(argc), char UU(*const argv[])) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);                                                           assert(r==0);
    r=db_env_create(&env, 0);                                                     CKERR(r);
    r=env->open(env, TOKU_TEST_FILENAME, DB_PRIVATE|DB_INIT_MPOOL|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);                         CKERR(r);
    r=db_create(&db, env, 0);                                                     CKERR(r);
    r=db->open(db, NULL, "doesnotexist.db", "testdb", DB_BTREE, 0, 0666);         assert(r==ENOENT);
    r=db->open(db, NULL, "doesnotexist.db", "testdb", DB_BTREE, DB_CREATE, 0666); CKERR(r);
    r=db->close(db, 0);                                                           CKERR(r);
    r=env->close(env, 0);                                                         CKERR(r);
    return 0;
}
