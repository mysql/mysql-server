/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <db.h>
#include <sys/stat.h>

DB_ENV *env;
DB *db;

static void
run (void) {
    int r;
    DB_TXN *txn;
    char v101=101, v102=102, v1=1, v2=2;
    int vN=0;
    int N=0;
    {
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	DBT k,v;
	int i;
	r=db->put(db, txn, dbt_init(&k, &v1, 1), dbt_init(&v, &v101, 1), DB_YESOVERWRITE); CKERR(r);
	r=db->put(db, txn, dbt_init(&k, &v2, 1), dbt_init(&v, &v102, 1), DB_YESOVERWRITE); CKERR(r);
	for (i=0; i<N; i++) {
	    int iv = htonl(i);
	    r=db->put(db, txn, dbt_init(&k, &vN, 1), dbt_init(&v, &iv, 4), DB_YESOVERWRITE); CKERR(r);
	}
	r=txn->commit(txn, 0);                                        CKERR(r);
    }

    {
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	DBC *c;
	r=db->cursor(db, txn, &c, 0);                                 CKERR(r);
	DBT k,v;
	int i;
 	for (i=0; i<N; i++) {
	    r=c->c_get(c, dbt_init_malloc(&k), dbt_init_malloc(&v), DB_NEXT); CKERR(r);
	    assert(k.size==1);          assert(v.size==4);
	    assert(*(char*)k.data==vN); assert((int)ntohl(*(int*)v.data)==i);
	}

	r=c->c_get(c, dbt_init(&k, 0, 0), dbt_init(&v, 0, 0), DB_NEXT);  CKERR(r);
	assert(*(char*)k.data==v1); assert(*(char*)v.data==v101);
	r=c->c_get(c, dbt_init(&k, 0, 0), dbt_init(&v, 0, 0), DB_NEXT);  CKERR(r);
	assert(*(char*)k.data==v2); assert(*(char*)v.data==v102);
	r=c->c_get(c, dbt_init(&k, 0, 0), dbt_init(&v, 0, 0), DB_NEXT);  assert(r!=0);
	r=c->c_close(c);                                                   CKERR(r);
	r=txn->commit(txn, 0);                                             CKERR(r);
    }
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);

    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    int r;

    DB_TXN *txn;
    {
        r = db_env_create(&env, 0);                                   CKERR(r);
	r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
	env->set_errfile(env, stderr);
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	r=db_create(&db, env, 0);                                     CKERR(r);
	r=db->set_flags(db, DB_DUP|DB_DUPSORT);
	r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);  CKERR(r);
	r=txn->commit(txn, 0);                                        CKERR(r);
    }
    run();
    {
	r=db->close(db, 0);                                           CKERR(r);
	r=env->close(env, 0);                                         CKERR(r);
    }

    return 0;
}
