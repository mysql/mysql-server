/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2008 Tokutek Inc.  All rights reserved."

#include <assert.h>
#include <portability.h>
#include <db.h>
#include <unistd.h>
#include <sys/stat.h>
#include "test.h"

static DB_ENV *env;
static DB_TXN *txn = 0;
static DB     *db = 0;

static void test (void) {
    u_int64_t limit=30000;
    int r;

    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777);       CKERR(r);
    r=db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, 0777); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=db->set_pagesize(db, 4096);
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
    r=txn->commit(txn, 0);    assert(r==0);

    u_int64_t i;
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    for (i=0; i<limit; i++) {
	char key[100],val[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	snprintf(val, 100, "%08llu", (unsigned long long)2*i+1);
	DBT k,v;
	r = db->put(db, txn, dbt_init(&k, key, 1+strlen(key)), dbt_init(&v,val, 1+strlen(val)), DB_YESOVERWRITE);
	assert(r == 0);
    }
    r=txn->commit(txn, 0);    assert(r==0);
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    // flatten it.
    for (i=0; i<limit; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	DBT k,v;
	memset(&v, 0, sizeof(v));
	r = db->get(db, txn, dbt_init(&k, key, 1+strlen(key)), &v, 0);
	assert(r==0);
    }
    for (i=0; i<limit; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	DBT k;
	u_int64_t less,equal,greater;
	int is_exact;
	r = db->key_range64(db, txn, dbt_init(&k, key, 1+strlen(key)), &less, &equal, &greater, &is_exact);
	assert(r == 0);
	//printf("key %llu/%llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*limit, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater);
	assert(less==(u_int64_t)i);
	assert(equal==1);
	assert(less+equal+greater == limit);
    }
    for (i=0; i<1+limit; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i);
	DBT k;
	u_int64_t less,equal,greater;
	int is_exact;
	r = db->key_range64(db, txn, dbt_init(&k, key, 1+strlen(key)), &less, &equal, &greater, &is_exact);
	assert(r == 0);
	//printf("key %llu/%llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*limit, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater);
	assert(less==(u_int64_t)i);
	assert(equal==0);
	assert(less+equal+greater == limit);
    }
    r=txn->commit(txn, 0);    assert(r==0);
    r = db->close(db, 0);     assert(r==0);
    r = env->close(env, 0);   assert(r==0);
}

int main (int argc , const char *argv[]) {
    parse_args(argc, argv);
    test();
    return 0;
}
