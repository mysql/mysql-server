/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"


#include <db.h>
#include <unistd.h>
#include <sys/stat.h>

static DB_ENV *env;
static DB_TXN *txn = 0;
static DB     *db = 0;

static void test (void) {
    const u_int64_t limit=10000;
    u_int64_t permute[limit];
    int r;

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);
    r=db_env_create(&env, 0); CKERR(r);
    // set the cachetable to size 0 so that things fit.
    r=env->set_cachesize(env, 0, 0, 1);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=db->set_pagesize(db, 4096);
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=txn->commit(txn, 0);    assert(r==0);

    u_int64_t i;
    // permute the numbers from 0 (inclusive) to limit (exclusive)
    permute[0]=0;
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    for (i=1; i<limit; i++) {
	permute[i]=i;
	int ra = random()%(i+1);
	permute[i]=permute[ra];
	permute[ra]=i;
    }
    r=txn->commit(txn, 0);    assert(r==0);
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    for (i=0; i<limit; i++) {
	char key[100],val[100];
	u_int64_t ri = permute[i];
	snprintf(key, 100, "%08llu", (unsigned long long)2*ri+1);
	snprintf(val, 100, "%08llu", (unsigned long long)2*ri+1);
	DBT k,v;
	r = db->put(db, txn, dbt_init(&k, key, 1+strlen(key)), dbt_init(&v,val, 1+strlen(val)), 0);
	assert(r == 0);
    }
    r=txn->commit(txn, 0);    assert(r==0);
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    // Don't flatten it.
    // search for the items that are there and those that aren't.
    // 
    u_int64_t prevless=0;
    u_int64_t prevgreater=limit;
    for (i=0; i<2*limit+1; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)i);
	DBT k;
	u_int64_t less,equal,greater;
	int is_exact;
	r = db->key_range64(db, txn, dbt_init(&k, key, 1+strlen(key)), &less, &equal, &greater, &is_exact);
	assert(r == 0);
	//printf("key %llu/%llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*limit, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater);
	assert(equal==0 || equal==1);
	assert(less>=prevless);	      prevless    = less;
	assert(greater<=prevgreater); prevgreater = greater;
    }
    r=txn->commit(txn, 0);    assert(r==0);
    r = db->close(db, 0);     assert(r==0);
    r = env->close(env, 0);   assert(r==0);
}

int
test_main (int argc , char * const argv[]) {
    parse_args(argc, argv);
    test();
    return 0;
}
