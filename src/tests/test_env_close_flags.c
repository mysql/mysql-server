/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <assert.h>
#include <db.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>


// DIR is defined in the Makefile

#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

int main (int argc __attribute__((__unused__)), char *argv[]  __attribute__((__unused__))) {
    DB_ENV *env;
    int r;
    system("rm -rf " DIR);
    r=mkdir(DIR, 0777);        assert(r==0);
    r=db_env_create(&env, 0);  assert(r==0);
    r=env->close   (env, 0);   assert(r==0);
    system("rm -rf " DIR);
    r=mkdir(DIR, 0777);        assert(r==0);
    r=db_env_create(&env, 0);  assert(r==0);
    r=env->close   (env, 1);  
    //BDB does not check this in some versions
#if defined(USE_TDB) || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
    assert(r==EINVAL);
#else
    assert(r==0);
#endif
    system("rm -rf " DIR); 
    r=mkdir(DIR, 0777);        assert(r==0);

    r=db_env_create(&env, 0);  assert(r==0);
    r=env->open(env, DIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_PRIVATE|DB_CREATE, 0777); CKERR(r);
    r=env->close   (env, 0);  assert(r==0);
    
    system("rm -rf " DIR); 
    r=mkdir(DIR, 0777);        assert(r==0);

    r=db_env_create(&env, 0);  assert(r==0);
    r=env->open(env, DIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_PRIVATE|DB_CREATE, 0777); CKERR(r);
    r=env->close   (env, 1);
    //BDB does not check this.
#if defined(USE_TDB) || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
    assert(r==EINVAL);
#else
    assert(r==0);
#endif
    return 0;
}
