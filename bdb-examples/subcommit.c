/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <db.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>

#define DIR "subcommit.dir"

DB_ENV *env;
DB *db;
DB_TXN *txn=0;

int main (int argc, char *argv[]) {
    int r;
    int i;
    r = system("rm -rf ./" DIR);
    r = mkdir(DIR, 0777);                assert(r==0);
    r = db_env_create(&env, 0);          assert(r==0);
    r = env->open(env, DIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_THREAD|DB_RECOVER|DB_PRIVATE, 0666); assert(r==0);
    r = db_create(&db, env, 0);          assert(r==0);
    r = env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r = db->open(db, txn, "t1.db", "main", DB_BTREE, DB_CREATE|DB_THREAD | (txn ? 0 : DB_AUTO_COMMIT), 0660); assert(r==0);
    for (i=0; i<1000; i++) {
	DB_TXN *subtxn;
	DBT a,b;
	int data=htonl(i);
	r = env->txn_begin(env, txn, &subtxn, 0); assert(r==0);
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	a.data  = b.data  = &data;
	a.flags = b.flags = 0;
	a.ulen  = b.ulen  = 0;
	a.size  = b.size   =  sizeof(data);
	r = db->put(db, subtxn, &a, &b, 0); if (r!=0) db->err(db, r, "%s:%d", __FILE__, __LINE__); assert(r==0);
	r = subtxn->commit(subtxn, 0);      assert(r==0);
    }
    if (txn) {
	r = txn->commit(txn, 0); assert(r==0);
    }
    r = db->close(db, 0);    assert(r==0);
    r = env->close(env, 0);  assert(r==0);
    return 0;
}
