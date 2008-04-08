/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Test what happens if we have deleted the first thing in the database.
 * Also the last.
 * Also if we've deleted a lot of stuff, so that the first good thing is not on the first page.
 */

#include <db.h>
#include <sys/stat.h>
#include "test.h"

static DB_ENV *env;
static DB *db;
static DB_TXN *txn;

// Got a different problem when N=1000.  

void insert (int i) {
    char hello[30], there[30];
    DBT key,data;
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(there, sizeof(there), "there%d", i);
    int r = db->put(db, txn,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    dbt_init(&data, there, strlen(there)+1),
		    0);
    CKERR(r);
}

void delete (int i) {
    char hello[30];
    DBT key;
    snprintf(hello, sizeof(hello), "hello%d", i);
    int r = db->del(db, txn,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    0);
    CKERR(r);
}

void find (int i) {
    char hello[30];
    DBT key, val;
    memset(&val,0,sizeof(val));
    snprintf(hello, sizeof(hello), "hello%d", i);
    int r = db->get(db, txn,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    &val,
		    0);
    CKERR(r);
}

void find_first (int i) {
    int r;
    DBC *cursor;
    DBT key, val;
    memset(&key,0,sizeof(key));    
    memset(&val,0,sizeof(val));

    r = db->cursor(db, txn, &cursor, 0);
    CKERR(r);
    r = cursor->c_get(cursor, &key, &val, DB_FIRST);
    assert(r==0);
    
    char hello[30], there[30];
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(there, sizeof(there), "there%d", i);

    assert(strcmp(hello, key.data)==0);
    assert(strcmp(there, val.data)==0);
}

void do_abort_delete_first(int N) {
    int r,i;
    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777);       assert(r==0);

    r=db_env_create(&env, 0); assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, 0777); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=db->set_pagesize(db, 4096); // Use a small page
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
    r=txn->commit(txn, 0);    assert(r==0);

    // First fill up the db
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);

    for (i=0; i<N; i++) {
	insert(i);
    }
    r=txn->commit(txn, 0); CKERR(r);

    // Now delete a bunch of stuff and see if we can do DB_FIRST
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    for (i=0; i<N-1; i++) {
	delete(i);
    }
    find(i);
    find_first(i);
    r=txn->commit(txn, 0); CKERR(r);


    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    do_abort_delete_first(10);
    do_abort_delete_first(1000);
    system("../../newbrt/brtdump " ENVDIR "/foo.db");
    return 0;
}
