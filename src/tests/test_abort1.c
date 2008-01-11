/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Simple test of logging.  Can I start a TokuDB with logging enabled? */
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <db.h>
#include <string.h>
#include <stdio.h>

// DIR is defined in the Makefile

DB_ENV *env;
DB *db;
DB_TXN *tid;

#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

int main (int argc, char *argv[]) {
    int r;
    system("rm -rf " DIR);
    r=mkdir(DIR, 0777);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, DIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_PRIVATE|DB_CREATE, 0777); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
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
    r=tid->abort(tid);        assert(r==0);
    r=db->close(db, 0);       assert(r==0);
    r=env->close(env, 0);     assert(r==0);
    return 0;
}
