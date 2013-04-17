/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

/* Can I close a db without opening it? */

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <db.h>


// ENVDIR is defined in the Makefile


int
test_main (int UU(argc), char UU(*const argv[])) {
  int r;
  DB_ENV *env;
  DB *db;
  r = system("rm -rf " ENVDIR);
  CKERR(r);
  r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
  r=db_env_create(&env, 0); assert(r==0);
  r=env->open(env, ENVDIR, DB_PRIVATE|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);
  r=db_create(&db, env, 0); assert(r==0);
  r = db->open(db, NULL, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

  // call hot_optimize on an empty db. The empty db should have only a root node, which should invoke the bug
  r = db->hot_optimize(db, NULL, NULL); assert_zero(r);
    
  r=db->close(db, 0);       assert(r==0);
  r=env->close(env, 0);     assert(r==0);
  return 0;
}
