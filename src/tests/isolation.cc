/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// Test that isolation works right for subtransactions.
// In particular, check to see what happens if a subtransaction has different isolation level from its parent.

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
    DB *db;
    {
	DB_TXN *txna;
	r = env->txn_begin(env, NULL, &txna, 0);                                        CKERR(r);

	r = db_create(&db, env, 0);                                                     CKERR(r);
	r = db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666);              CKERR(r);

	DBT key,val;
	r = db->put(db, txna, dbt_init(&key, "a", 2), dbt_init(&val, "a", 2), 0);       CKERR(r);

	r = txna->commit(txna, 0);                                                      CKERR(r);
    }
    DB_TXN *txna, *txnx;
    r = env->txn_begin(env, NULL, &txna, DB_READ_UNCOMMITTED);                          CKERR(r);
    r = env->txn_begin(env, NULL, &txnx, 0);                                            CKERR(r);

    // X writes a value, and B tries to read it in uncommitted
    {
//	DB_TXN *txnb;
//	r = env->txn_begin(env, txna, &txnb, DB_READ_UNCOMMITTED);                      CKERR(r);
	{
	    DBT key,val;
	    r = db->put(db, txnx, dbt_init(&key, "x", 2), dbt_init(&val, "x", 2), 0);   CKERR(r);
	    dbt_init_malloc(&val);
	    r = db->get(db, txna, dbt_init(&key, "x", 2), &val, 0);                     CKERR(r);
            toku_free(val.data);
            val.data = NULL;
	}
//	r = txnb->commit(txnb, 0);                                                      CKERR(r);
    }
    r = txna->commit(txna, 0);                                                          CKERR(r);
    r = txnx->commit(txnx, 0);                                                          CKERR(r);

    r = db->close(db, 0);                                                               CKERR(r);
    r = env->close(env, 0);                                                             CKERR(r);
    
    return 0;
}
