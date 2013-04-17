/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Can I close a db without opening it? */
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <toku_portability.h>
#include <db.h>

#include "test.h"

// ENVDIR is defined in the Makefile

DB_ENV *env;
DB *db;

int
test_main (int UU(argc), const char UU(*argv[])) {
    int r;
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, ENVDIR, DB_PRIVATE|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);
    r=db_create(&db, env, 0); assert(r==0);
    r=db->close(db, 0);       assert(r==0);
    r=env->close(env, 0);     assert(r==0);
    return 0;
}
