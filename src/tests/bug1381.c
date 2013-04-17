/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
/* Test for #1381:  If we insert into a locked empty table, not much goes into the rollback data structure. */

#include <db.h>
#include <sys/stat.h>
#include <memory.h>

static void do_1381_maybe_lock (int do_table_lock, u_int64_t *raw_count) {
    int r;
    DB_TXN * const null_txn = 0;

    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);


    // Create an empty file
    {
	DB_ENV *env;
	DB *db;
	
	const int envflags = DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK|DB_THREAD|DB_PRIVATE;

	r = db_env_create(&env, 0);                                           CKERR(r);
	r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);

	r = db_create(&db, env, 0);                                           CKERR(r);
	r = db->open(db, null_txn, "main", 0,     DB_BTREE, DB_CREATE, 0666); CKERR(r);

	r = db->close(db, 0);                                                 CKERR(r);
	r = env->close(env, 0);                                               CKERR(r);
    }
    // Now open the empty file and insert
    {
	DB_ENV *env;
	DB *db;
	const int envflags = DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK|DB_THREAD |DB_PRIVATE;
	
	r = db_env_create(&env, 0);                                           CKERR(r);
	r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);

	r = db_create(&db, env, 0);                                           CKERR(r);
	r = db->open(db, null_txn, "main", 0,     DB_BTREE, 0, 0666);         CKERR(r);

	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0);                              CKERR(r);

	if (do_table_lock) {
	    r = db->pre_acquire_table_lock(db, txn);                      CKERR(r);
	}

	struct txn_stat *s1, *s2;
	r = txn->txn_stat(txn, &s1);                                      CKERR(r);

	{
	    DBT key={.data="hi", .size=3};
	    DBT val={.data="v",    .size=2};
	    r = db->put(db, txn, &key, &val, 0);                          CKERR(r);
	}

	r = txn->txn_stat(txn, &s2);                                      CKERR(r);
	//printf("Raw counts = %" PRId64 ", %" PRId64 "\n", s1->rolltmp_raw_count, s2->rolltmp_raw_count);

	*raw_count = s2->rolltmp_raw_count - s1->rolltmp_raw_count;
	if (do_table_lock) {
	    assert(s1->rolltmp_raw_count == s2->rolltmp_raw_count);
	} else {
	    assert(s1->rolltmp_raw_count < s2->rolltmp_raw_count);
	}
	
	toku_free(s1); toku_free(s2);

	r = txn->commit(txn, 0);                                              CKERR(r);

	r = db->close(db, 0);                                                 CKERR(r);
	r = env->close(env, 0);                                               CKERR(r);
    }
}

static void
do_1381 (void) {
    int do_table_lock;
    u_int64_t raw_counts[2];
    for (do_table_lock = 0; do_table_lock < 2 ; do_table_lock++) {
	do_1381_maybe_lock(do_table_lock, &raw_counts[do_table_lock]);
    }
    assert(raw_counts[0] > raw_counts[1]); // the raw counts should be less for the tablelock case. 
}

int
test_main (int argc, char * const argv[])
{
    parse_args(argc, argv);
    do_1381();
    return 0;
}
