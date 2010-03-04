/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"



#include <db.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>


// ENVDIR is defined in the Makefile

int
test_main (int argc __attribute__((__unused__)), char *const argv[]  __attribute__((__unused__))) {
    DB_ENV *env;
    int r;
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);        assert(r==0);
    r=db_env_create(&env, 0);  assert(r==0);
    env->set_errfile(env,0); // Turn off those annoying errors
    r=env->close   (env, 0);   assert(r==0);
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);        assert(r==0);
    r=db_env_create(&env, 0);  assert(r==0);
    env->set_errfile(env,0); // Turn off those annoying errors
    r=env->close   (env, 1);  
    //BDB does not check this in some versions
#if defined(USE_TDB) || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
    assert(r==EINVAL);
#else
    assert(r==0);
#endif
    system("rm -rf " ENVDIR); 
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);        assert(r==0);

    r=db_env_create(&env, 0);  assert(r==0);
    env->set_errfile(env,0); // Turn off those annoying errors
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_PRIVATE|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=env->close   (env, 0);  assert(r==0);
    
    system("rm -rf " ENVDIR); 
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);        assert(r==0);

    r=db_env_create(&env, 0);  assert(r==0);
    env->set_errfile(env,0); // Turn off those annoying errors
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_PRIVATE|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=env->close   (env, 1);
    //BDB does not check this.
#if defined(USE_TDB) || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
    assert(r==EINVAL);
#else
    assert(r==0);
#endif
    return 0;
}
