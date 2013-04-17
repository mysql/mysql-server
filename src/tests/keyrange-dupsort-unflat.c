/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <assert.h>
#include <db.h>
#include <unistd.h>
#include <sys/stat.h>

static DB_ENV *env;
static DB_TXN *txn = 0;
static DB     *db = 0;

static void test (void) {
    u_int64_t limit=100;
    u_int64_t ilimit=100;
    int r;
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);
    r=db_env_create(&env, 0); CKERR(r);
    // set the cachetable to size 1 so that things won't fit.
    r=env->set_cachesize(env, 0, 1, 1);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=db->set_flags(db, DB_DUP|DB_DUPSORT); assert(r==0);
    r=db->set_pagesize(db, 4096);
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=txn->commit(txn, 0);    assert(r==0);

    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    u_int64_t i;
    for (i=0; i<limit; i++) {
	u_int64_t j;
	for (j=0; j<ilimit; j++) {
	    char key[100],val[100];
	    snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	    snprintf(val, 100, "%08llu", (unsigned long long)2*j+1);
	    DBT k,v;
	    r = db->put(db, txn, dbt_init(&k, key, 1+strlen(key)), dbt_init(&v,val, 1+strlen(val)), DB_YESOVERWRITE);
	    assert(r == 0);
	}
    }
    r=txn->commit(txn, 0);    assert(r==0);
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    u_int64_t prevless = 0;
    u_int64_t prevgreater = limit*ilimit;
    for (i=0; i<2*limit+1; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)i);
	DBT k;
	u_int64_t less,equal,greater;
	int is_exact;
	r = db->key_range64(db, txn, dbt_init(&k, key, 1+strlen(key)), &less, &equal, &greater, &is_exact);
	assert(r == 0);
	//printf("key %llu/%llu %llu %llu %llu\n", (unsigned long long)2*i+1, (unsigned long long)2*limit+1, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater);
	assert(less<=ilimit*i);
	assert(equal<=ilimit);
	assert(less+equal+greater <= limit*ilimit);
	assert(less>=prevless); prevless=less;
	assert(greater<=prevgreater); prevgreater=greater;
    }
    r=txn->commit(txn, 0);    assert(r==0);
    r = db->close(db, 0);     assert(r==0);
    r = env->close(env, 0);   assert(r==0);
}

int
test_main (int argc , char *argv[]) {
    parse_args(argc, argv);
    test();
    return 0;
}
