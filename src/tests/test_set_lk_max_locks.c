/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Test to see if the set_lk_max_locks works. */
/* This is very specific to TokuDB.  It won't work with Berkeley DB. */

#include <assert.h>
#include <db.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include "test.h"

// ENVDIR is defined in the Makefile

static void make_db (int n_locks) {
    DB_ENV *env;
    DB *db;
    DB_TXN *tid, *tid2;
    int r;
    int i;

    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    env->set_errfile(env, 0);
    if (n_locks>0) {
	r=env->set_lk_max_locks(env, n_locks); CKERR(r);
        /* test the get_lk_max_locks method */
        u_int32_t set_locks;
#ifdef TOKUDB
	// BDB cannot handle a NULL passed to get_lk_max_locks
        r=env->get_lk_max_locks(env, 0); 
        assert(r == EINVAL);
#endif
        r=env->get_lk_max_locks(env, &set_locks);
        assert(r == 0 && set_locks == n_locks);
    }
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, 0777); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
    r=tid->commit(tid, 0);    assert(r==0);
    
#ifndef TOKUDB
    u_int32_t pagesize;
    r = db->get_pagesize(db, &pagesize); CKERR(r);
    u_int32_t datasize = pagesize/6;
#else
    u_int32_t datasize = 1;
#endif
    int effective_n_locks = (n_locks<0) ? 1000 : n_locks;
    // create even numbered keys 0 2 4 ...  (effective_n_locks*32-2)
    
    r=env->txn_begin(env, 0, &tid, 0);    CKERR(r);
    for (i=0; i<effective_n_locks*16; i++) {
	char hello[30], there[datasize+30];
	DBT key,data;
	snprintf(hello, sizeof(hello), "hello%09d", 2*i);
	snprintf(there, sizeof(there), "there%d%0*d", 2*i, datasize, 2*i); // For BDB this is chosen so that different locks are on different pages
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data  = hello; key.size=strlen(hello)+1;
	data.data = there; data.size=strlen(there)+1;
	if (i%50==49) {
	    r=tid->commit(tid, 0);                CKERR(r);
	    r=env->txn_begin(env, 0, &tid, 0);    CKERR(r);
	}
	r=db->put(db, tid, &key, &data, 0);   CKERR(r);
    }
    r=tid->commit(tid, 0);                CKERR(r);

    // Now using two different transactions have one transaction create keys
    //   1 17 33 ... (1 mod 16)
    // and another do
    //   9 25 41 ... (9 mod 16)

    r=env->txn_begin(env, 0, &tid, 0);    CKERR(r);
    r=env->txn_begin(env, 0, &tid2, 0);   CKERR(r);
#if 1

    for (i=0; i<effective_n_locks*2; i++) {
	int j;
	for (j=0; j<2; j++) {
	    char hello[30], there[datasize+30];
	    DBT key,data;
	    int num = 16*i+8*j+1;
	    snprintf(hello, sizeof(hello), "hello%09d", num);
	    snprintf(there, sizeof(there), "there%d%*d", num, datasize, num); // For BDB this is chosen so that different locks are on different pages
	    memset(&key, 0, sizeof(key));
	    memset(&data, 0, sizeof(data));
	    //printf("Writing %s in %d\n", hello, j);
	    key.data  = hello; key.size=strlen(hello)+1;
	    data.data = there; data.size=strlen(there)+1;
	    r=db->put(db, j==0 ? tid : tid2, &key, &data, 0);
#ifdef TOKUDB
	    // Lock escalation cannot help here:  We require too many locks because we are alternating between tid and tid2
	    if (i*2+j<effective_n_locks) {
		CKERR(r);
	    } else assert(r==ENOMEM);
#else
	    if (i*2+j+2<effective_n_locks) {
		if (r!=0) printf("r=%d on i=%d j=%d eff=%d\n", r, i, j, effective_n_locks);
		CKERR(r);
	    }
	    else assert(r==ENOMEM);
#endif
	}
    }
#endif
    r=tid->commit(tid2, 0);   assert(r==0);
    r=tid->commit(tid, 0);    assert(r==0);
    r=db->close(db, 0);       assert(r==0);
    r=env->close(env, 0);     assert(r==0);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    make_db(-1);
    make_db(100); return 0;
    make_db(1000);
    make_db(2000);
    return 0;
}
