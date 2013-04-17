/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: checkpoint_1.c 18673 2010-03-21 04:18:21Z bkuszmaul $"
#include "test.h"
#include <db.h>
#include <sys/stat.h>

#include "test.h"

int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);

    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    DB_ENV *env;
    r = db_env_create(&env, 0);
        CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_RECOVER;
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);

    DB_ENV *env2;
    r = db_env_create(&env2, 0);
        CKERR(r);
    r = env2->open(env2, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR2(r, EWOULDBLOCK);

    r = env->close(env, 0);
        CKERR(r);

    r = env2->open(env2, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);

    r = env2->close(env2, 0);
        CKERR(r);
    return 0;
}

