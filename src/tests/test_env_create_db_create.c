/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <db.h>

int main (int argc, char *argv[]) {
    DB_ENV *env;
    DB *db;
    int r;
    r = db_env_create(&env, 0);
    assert(r == 0);
    r = db_create(&db, env, 0); 
// BDB doesnt' actually barf on this case.
#ifdef USE_TDB
    assert(r != 0);
#else
    r = db->close(db, 0);
    assert(r == 0);    
#endif
    r = env->close(env, 0);       
    assert(r == 0);
    return 0;
}
