/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Test to see if we can do logging and recovery. */
/* This is very specific to TokuDB.  It won't work with Berkeley DB. */
/* This test_log10 inserts to a db, closes, reopens, and inserts more to db.  We want to make sure that the recovery of the buffers works. */
/* Lots of stuff gets inserted. */

#include <assert.h>
#include <portability.h>
#include <db.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

// ENVDIR is defined in the Makefile

#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

struct in_db;
struct in_db {
    long int r;
    int i;
    struct in_db *next;
} *items=0;

int maxcount = 10000;

static void insert_some (int outeri) {
    u_int32_t create_flag = outeri%2 ? DB_CREATE : 0; // Sometimes use DB_CREATE, sometimes don't.
    int r;
    DB_ENV *env;
    DB *db;
    DB_TXN *tid;
    r=db_env_create(&env, 0); assert(r==0);
    r=env->set_lk_max_locks(env, 2*maxcount); CKERR(r);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|create_flag, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, create_flag, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=tid->commit(tid, 0);    assert(r==0);

    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    
    int i;
    for (i=0; i<maxcount; i++) {
	char hello[30], there[30];
	DBT key,data;
	struct in_db *newitem = malloc(sizeof(*newitem));
	newitem->r = random();
	newitem->i = i;
	newitem->next = items;
	items = newitem;
	snprintf(hello, sizeof(hello), "hello%ld.%d.%d", newitem->r, outeri, newitem->i);
	snprintf(there, sizeof(hello), "there%d", i);
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data  = hello; key.size=strlen(hello)+1;
	data.data = there; data.size=strlen(there)+1;
	r=db->put(db, tid, &key, &data, 0);  CKERR(r);
#ifndef TOKUDB
	// BDB cannot handle such a big txn.
	if (i%1000==999) {
	    r=tid->commit(tid, 0);    assert(r==0);
	    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
	}
#endif
    }
    r=tid->commit(tid, 0);    assert(r==0);
    r=db->close(db, 0);       assert(r==0);
    r=env->close(env, 0);     assert(r==0);
}    

static void make_db (void) {
    DB_ENV *env;
    DB *db;
    DB_TXN *tid;
    int r;
    int i;

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->set_lk_max_locks(env, 2*maxcount); CKERR(r);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=tid->commit(tid, 0);    assert(r==0);
    r=db->close(db, 0);  CKERR(r);
    r=env->close(env, 0); CKERR(r);

    for (i=0; i<10; i++)
	insert_some(i);
    
    while (items) {
	struct in_db *next=items->next;
	free(items);
	items=next;
    }
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    make_db();
    return 0;
}
