/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id$"
#include "test.h"
/* Test for #1381:  If we insert into a locked empty table, not much goes into the rollback data structure. */

#include <db.h>
#include <sys/stat.h>
#include <memory.h>

static int generate_row_for_put(
    DB *UU(dest_db), 
    DB *UU(src_db), 
    DBT *dest_key, 
    DBT *dest_val, 
    const DBT *src_key, 
    const DBT *src_val
    ) 
{    
    dest_key->data = src_key->data;
    dest_key->size = src_key->size;
    dest_key->flags = 0;
    dest_val->data = src_val->data;
    dest_val->size = src_val->size;
    dest_val->flags = 0;
    return 0;
}


static void do_1381_maybe_lock (int do_loader, uint64_t *raw_count) {
    int r;
    DB_TXN * const null_txn = 0;

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);


    // Create an empty file
    {
	DB_ENV *env;
	DB *db;
	
	const int envflags = DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK|DB_THREAD|DB_PRIVATE;

	r = db_env_create(&env, 0);                                           CKERR(r);
	r = env->set_redzone(env, 0);                                         CKERR(r);
        r = env->set_generate_row_callback_for_put(env, generate_row_for_put); CKERR(r);
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
	r = env->set_redzone(env, 0);                                         CKERR(r);
        r = env->set_generate_row_callback_for_put(env, generate_row_for_put); CKERR(r);
	r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);

	r = db_create(&db, env, 0);                                           CKERR(r);
	r = db->open(db, null_txn, "main", 0,     DB_BTREE, 0, 0666);         CKERR(r);

	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0);                              CKERR(r);
        uint32_t mult_put_flags = 0;
        uint32_t mult_dbt_flags = 0;
        DB_LOADER* loader = NULL;
	if (do_loader) {
	    r = env->create_loader(
                env, 
                txn, 
                &loader, 
                NULL, // no src_db needed
                1, 
                &db, 
                &mult_put_flags,
                &mult_dbt_flags,
                LOADER_COMPRESS_INTERMEDIATES
                );
            CKERR(r);
	}

	struct txn_stat *s1, *s2;
	r = txn->txn_stat(txn, &s1);                                      CKERR(r);

	{
	    DBT key;
            dbt_init(&key, "hi", 3);
	    DBT val;
            dbt_init(&val, "v", 2);
            if (do_loader) {
                r = loader->put(loader, &key, &val);
                CKERR(r);
            }
            else {
	        r = db->put(db, txn, &key, &val, 0);
                CKERR(r);
            }
	}
        if (do_loader) {
            r = loader->close(loader);
            CKERR(r);
        }

	r = txn->txn_stat(txn, &s2);                                      CKERR(r);
	//printf("Raw counts = %" PRId64 ", %" PRId64 "\n", s1->rollback_raw_count, s2->rollback_raw_count);

	*raw_count = s2->rollback_raw_count - s1->rollback_raw_count;
	if (do_loader) {
	    assert(s1->rollback_raw_count < s2->rollback_raw_count);
            assert(s1->rollback_num_entries + 1 == s2->rollback_num_entries);
	} else {
	    assert(s1->rollback_raw_count < s2->rollback_raw_count);
            assert(s1->rollback_num_entries < s2->rollback_num_entries);
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
    uint64_t raw_counts[2];
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
