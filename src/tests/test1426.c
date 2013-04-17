/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
/* Test for #1426. Make sure deletes and inserts in a FIFO work. */
/* This test is run using a special makefile rule that runs the TDB version and the BDB version, dumps their outputs, and compares them */

#include <db.h>
#include <memory.h>
#include <fcntl.h>

// |DB_INIT_TXN| DB_INIT_LOG  | DB_RECOVER
const int envflags = DB_CREATE|DB_INIT_MPOOL|DB_INIT_LOCK |DB_THREAD |DB_PRIVATE;

DB_ENV *env;
DB     *db;
DB_TXN * const null_txn = NULL;

static void
empty_cachetable (void)
// Make all the cachetable entries clean.
// Brute force it by closing and reopening everything.
{
    int r;
    r = db->close(db, 0);                                                 CKERR(r);
    r = env->close(env, 0);                                               CKERR(r);
    r = db_env_create(&env, 0);                                           CKERR(r);
#ifdef TOKUDB
    r = env->set_cachesize(env, 0, 10000000, 1);                          CKERR(r);
#endif
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);
    r = db_create(&db, env, 0);                                           CKERR(r);
    r = db->open(db, null_txn, "main", 0,     DB_BTREE, 0, 0666);         CKERR(r);
}

static void
do_insert_delete_fifo (void)
{
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    
    int r;
    r = db_env_create(&env, 0);                                           CKERR(r);
#ifdef TOKUDB
    r = env->set_cachesize(env, 0, 10000000, 1);                          CKERR(r);
#endif
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);
    r = db_create(&db, env, 0);                                           CKERR(r);
    r = db->set_pagesize(db, 4096);                                       CKERR(r);
    r = db->open(db, null_txn, "main", 0,     DB_BTREE, DB_CREATE, 0666); CKERR(r);
    {
	u_int64_t i;
	u_int64_t n_deleted = 0;
	u_int64_t N=20000; // total number to insert
	u_int64_t M= 5000; // size of rolling fifo
	u_int64_t D=  200; // number to delete at once
	for (i=0; i<N; i++) {
	    {
		char k[100],v[100];
		int keylen = snprintf(k, sizeof k, "%016" PRIu64 "key", i);
                u_int32_t rand1 = myrandom();
                u_int32_t rand2 = myrandom();
                u_int32_t rand3 = myrandom();
		int vallen = snprintf(v, sizeof v, "%016" PRIu64 "val%08x%08x%08x", i, rand1, rand2, rand3);
		DBT kt, vt;
		r = db->put(db, null_txn, dbt_init(&kt, k, keylen) , dbt_init(&vt, v, vallen), DB_YESOVERWRITE);    CKERR(r);
	    }
	    if (i%D==0) {
		// Once every D steps, delete everything until there are only M things left.
		// Flush the data down the tree for all the values we will do
		{
		    u_int64_t peek_here = n_deleted;
		    while (peek_here + M < i) {
			char k[100];
			int keylen = snprintf(k, sizeof k, "%016" PRIu64 "key", peek_here);
			DBT kt;
			DBT vt;
			memset(&vt, 0, sizeof(vt));
			vt.flags = DB_DBT_MALLOC;
			r = db->get(db, null_txn, dbt_init(&kt, k, keylen), &vt, 0); CKERR(r);
			peek_here++;
			toku_free(vt.data);
		    }
		}
		empty_cachetable();
		while (n_deleted + M < i) {
		    char k[100];
		    int keylen = snprintf(k, sizeof k, "%016" PRIu64 "key", n_deleted);
		    DBT kt;
		    r = db->del(db, null_txn, dbt_init(&kt, k, keylen), 0);
		    if (r!=0) printf("error %d %s", r, db_strerror(r));
		    CKERR(r);
		    n_deleted++;
		    empty_cachetable();
		}
	    }
	}
    }
    r = db->close(db, 0);                                                 CKERR(r);
    r = env->close(env, 0);                                               CKERR(r);
}

int
test_main (int argc, char *argv[])
{
    parse_args(argc, argv);
    do_insert_delete_fifo();
    return 0;
}

