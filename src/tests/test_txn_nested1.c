/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>
#include "tokuconst.h"
#define MAX_NEST MAX_NESTED_TRANSACTIONS


/*********************
 *
 * Purpose of this test is to exercise nested transactions in a basic way:
 * Create MAX nested transactions, inserting a value at each level, verify:
 * 
 * for i = 1 to MAX
 *  - txnid = begin()
 *  - txns[i] = txnid
 *  - insert, query
 *
 * for i = 1 to MAX
 *  - txnid = txns[MAX - i - 1]
 *  - commit or abort(txnid), query
 *
 */

static DB *db;
static DB_ENV *env;

static void
setup_db (void) {
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    r = db_env_create(&env, 0); CKERR(r);
    r = env->open(env, ENVDIR, DB_INIT_MPOOL | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_TXN | DB_PRIVATE | DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    CKERR(r);

    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = db->set_bt_compare(db, int_dbt_cmp); CKERR(r);
        r = db->set_dup_compare(db, int_dbt_cmp); CKERR(r);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0); CKERR(r);
    }
}


static void
close_db (void) {
    int r;
    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
}

static void
test_txn_nesting (int depth) {
    int r;
    if (verbose) { fprintf(stderr, "%s (%s):%d [depth = %d]\n", __FILE__, __FUNCTION__, __LINE__, depth); fflush(stderr); }

    DBT key, val, observed_val;
    dbt_init(&observed_val, NULL, 0);
    int i;

    DB_TXN * txns[depth];
    DB_TXN * parent = NULL;

    int vals[depth];

    int mykey = 42;
    dbt_init(&key, &mykey, sizeof mykey);
    

    for (i = 0; i < depth; i++){
	DB_TXN * this_txn;

	if (verbose)
	    printf("Begin txn at level %d\n", i);
	vals[i] = i;
	dbt_init(&val, &vals[i], sizeof i);
	r = env->txn_begin(env, parent, &this_txn, 0);   CKERR(r);
	txns[i] = this_txn;
	parent = this_txn;  // will be parent in next iteration
	r = db->put(db, this_txn, &key, &val, DB_YESOVERWRITE);          CKERR(r);

        r = db->get(db, this_txn, &key, &observed_val, 0); CKERR(r);
	assert(int_dbt_cmp(db, &val, &observed_val) == 0);
    }

    int which_val = depth-1;
    for (i = depth-1; i >= 0; i--) {
        //Query, verify the correct value is stored.
        //Close (abort/commit) innermost transaction

        if (verbose)
            printf("Commit txn at level %d\n", i);

        dbt_init(&observed_val, NULL, 0);
        r = db->get(db, txns[i], &key, &observed_val, 0); CKERR(r);
	dbt_init(&val, &vals[which_val], sizeof i);
	assert(int_dbt_cmp(db, &val, &observed_val) == 0);

	if (i % 2) {
	    r = txns[i]->commit(txns[i], DB_TXN_NOSYNC);   CKERR(r);
	    //which_val does not change (it gets promoted)
	}
	else {
	    r = txns[i]->abort(txns[i]); CKERR(r);
	    which_val = i - 1;
	}
        txns[i] = NULL;
    }
    //Query, verify the correct value is stored.
    r = db->get(db, NULL, &key, &observed_val, 0);
    if (which_val == -1) CKERR2(r, DB_NOTFOUND);
    else {
        CKERR(r);
	dbt_init(&val, &vals[which_val], sizeof i);
        assert(int_dbt_cmp(db, &val, &observed_val) == 0);
    }
}


#if 0
static void
test_txn_abort (int insert, int secondnum) {
    if (verbose) { fprintf(stderr, "%s (%s):%d [%d,%d]\n", __FILE__, __FUNCTION__, __LINE__, insert, secondnum); fflush(stderr); }
    setup_db();

    DBT key, val;
    int r;


    DB_TXN *parent = NULL, *child = NULL;

    int i = 1;
    r = env->txn_begin(env, 0, &parent, 0); CKERR(r);

    //Insert something as a child
    r = env->txn_begin(env, parent, &child, 0); CKERR(r);
    i = 1;
    r = db->put(db, child, dbt_init(&key, &i, sizeof i), dbt_init(&val, &i, sizeof i), 0); 
    CKERR(r);
    r = child->commit(child,DB_TXN_NOSYNC); 
    child = NULL;


    //delete it as a child
    r = env->txn_begin(env, parent, &child, 0); CKERR(r);
    i = secondnum;
    if (insert) {
        r = db->put(db, child, dbt_init(&key, &i, sizeof i), dbt_init(&val, &i, sizeof i), 0); 
        CKERR(r);
    }
    else { // delete
        r = db->del(db, child, dbt_init(&key, &i, sizeof i), DB_DELETE_ANY); 
	if (IS_TDB) {
	    CKERR(r);
	} else {
	    CKERR2(r, (secondnum==1 ? 0 : DB_NOTFOUND));
	}
    }
    r = child->commit(child,DB_TXN_NOSYNC); 
    child = NULL;

    r = parent->abort(parent);
    CKERR(r);
    parent = NULL;


    {
        DB_TXN *txn = NULL;
        /* walk the db, should be empty */
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); CKERR(r);
        memset(&key, 0, sizeof key);
        memset(&val, 0, sizeof val);
        r = cursor->c_get(cursor, &key, &val, DB_FIRST); 
        CKERR2(r, DB_NOTFOUND);
        r = cursor->c_close(cursor); CKERR(r);
        r = txn->commit(txn, 0);
    }
    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);

}

#endif

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    setup_db();
    test_txn_nesting(MAX_NEST);
    close_db();
    return 0;
}
