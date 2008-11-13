/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Simple test of logging.  Can I start a TokuDB with logging enabled? */
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <portability.h>
#include <db.h>

#include "test.h"

// ENVDIR is defined in the Makefile

DB_ENV *env;
DB *db;

int main (int UU(argc), char UU(*argv[])) {
    int r;
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                                                           assert(r==0);
    r=db_env_create(&env, 0);                                                     CKERR(r);
    r=env->open(env, ENVDIR, DB_PRIVATE|DB_INIT_MPOOL|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);                         CKERR(r);
    r=db_create(&db, env, 0);                                                     CKERR(r);
    r=db->open(db, NULL, "doesnotexist.db", "testdb", DB_BTREE, 0, 0666);         assert(r==ENOENT);
    r=db->open(db, NULL, "doesnotexist.db", "testdb", DB_BTREE, DB_CREATE, 0666); CKERR(r);
    r=db->close(db, 0);                                                           CKERR(r);
    r=env->close(env, 0);                                                         CKERR(r);
    return 0;
}
