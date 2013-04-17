/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id$"

/* Purpose of this test is to verify correct behavior of
 * zombie dbs.  
 *
 * A db is destroyed when it is closed by the user and there are no txns using it.
 * If a transaction creates a db and then closes, that leaves an open db with 
 * no transaction associated with it.  If another transaction then uses the db,
 * and then closes it, then that leaves a zombie db.  The db is closed, but 
 * cannot be destroyed because there is still a transaction associated with it
 * (not the transaction that created it).
 *
 * Outline of this test:
 *
 * begin txn_a
 * create db for new dictionary "foo"
 * commit txn_a
 *  => leaves open db with no txn
 *     (releases range lock on "foo" dname in directory)
 * 
 * begin txn_b
 * insert into db
 * close db
 *   => leaves zombie db, held open by txn_b
 * 
 * 
 * create txn_c
 * 
 * test1:
 * try to delete dictionary (env->dbremove(foo)) 
 *   should return DB_LOCK_NOT_GRANTED because txnB is holding range lock on some part of 
 *   the dictionary ("foo") referred to by db
 * 
 * test2:
 * try to rename dictionary (env->dbrename(foo->bar))
 *   should return DB_LOCK_NOT_GRANTED because txnB is holding range lock on some part of 
 *   the dictionary ("foo") referred to by db
 * 
 */

#include "test.h"
#include <db.h>

static DB_ENV *env;
static DB * db;

static void
setup (void) {
    int r;
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);

    r=db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
}



static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
}

static void
test_zombie_db(void) {
    int r;
    DBT key, val;
    DB_TXN * txn_b;

    r=env->txn_begin(env, 0, &txn_b, 0); CKERR(r);

    {
	DB_TXN * txn_a;
	dbt_init(&key, "key1", 4);
	dbt_init(&val, "val1", 4);

	r=env->txn_begin(env, 0, &txn_a, 0); CKERR(r);
	r=db_create(&db, env, 0); CKERR(r);
	r=db->open(db, txn_a, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
	r=db->put(db, txn_a, &key, &val, DB_YESOVERWRITE); CKERR(r);
	r=txn_a->commit(txn_a, 0); CKERR(r);
    }

    // db is now open with no associated txn

    {
	dbt_init(&key, "key2", 4);
	dbt_init(&val, "val2", 4);

	r = db->put(db, txn_b, &key, &val, DB_YESOVERWRITE); CKERR(r);
	r=db->close(db, 0); CKERR(r);
    }
    
    // db is now closed, but cannot be destroyed until txn_b closes
    // db is now a zombie

    {
	DB_TXN * txn_c;

	r=env->txn_begin(env, 0, &txn_c, 0); CKERR(r);
	r = env->dbremove(env, txn_c, "foo.db", NULL, 0);  
	CKERR2(r, DB_LOCK_NOTGRANTED);
	r = env->dbrename(env, txn_c, "foo.db", NULL, "bar.db", 0); 
	CKERR2(r, DB_LOCK_NOTGRANTED);
	r=txn_c->commit(txn_c, 0); CKERR(r);
    }
    
    r=txn_b->commit(txn_b, 0); CKERR(r);
    
    // db should now be destroyed
}

int
test_main (int argc, char *argv[]) {
    parse_args(argc, argv);
    setup();
    test_zombie_db();
    test_shutdown();
    return 0;
}
