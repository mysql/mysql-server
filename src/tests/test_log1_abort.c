/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

/* Do test_log1, except abort instead of commit. */


#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/stat.h>
#include <unistd.h>


// ENVDIR is defined in the Makefile

DB_ENV *env;
DB *db;
DB_TXN *tid;

int
test_main (int UU(argc), char UU(*argv[])) {
    int r;
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_PRIVATE|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    {
	DBT key,data;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data="hello";
	key.size=6;
	data.data="there";
	data.size=6;
	r=db->put(db, tid, &key, &data, 0);
	CKERR(r);
    }
    r=db->close(db, 0);       
    assert(r==0);
    r=tid->abort(tid);    
    assert(r==0);
    r=env->close(env, 0);
#ifdef USE_BDB
    assert(r==ENOENT);
#else
    assert(r==0);
#endif
    {
	toku_struct_stat statbuf;
	r = toku_stat(ENVDIR "/foo.db", &statbuf);
	assert(r==-1);
	assert(errno==ENOENT);
    }
    return 0;
}
