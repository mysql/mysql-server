/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "test.h"

/* Like test_log5 except abort. */

#include <assert.h>
#include <toku_portability.h>
#include <db.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <memory.h>

// ENVDIR is defined in the Makefile

struct in_db;
struct in_db {
    long int r;
    int i;
    struct in_db *next;
} *items=0;

static void make_db (void) {
    DB_ENV *env;
    DB *db;
    DB_TXN *tid;
    int r;
    int i;

    int maxcount = 24073;

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->set_lk_max_locks(env, 2*maxcount); CKERR(r);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=tid->commit(tid, 0);    assert(r==0);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    
    for (i=0; i<maxcount; i++) {
	char hello[30], there[30];
	DBT key,data;
	struct in_db *newitem = toku_malloc(sizeof(*newitem));
	newitem->r = random();
	newitem->i = i;
	newitem->next = items;
	items = newitem;
	snprintf(hello, sizeof(hello), "hello%ld.%d", newitem->r, newitem->i);
	snprintf(there, sizeof(hello), "there%d", i);
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data  = hello; key.size=strlen(hello)+1;
	data.data = there; data.size=strlen(there)+1;
	r=db->put(db, tid, &key, &data, 0);  assert(r==0);
    }
    r=tid->abort(tid);    assert(r==0);
    {
	struct in_db *l=items;
	for (l=items; l; l=l->next) {
	    char hello[30];
	    DBT key,data;
	    snprintf(hello, sizeof(hello), "hello%ld.%d", l->r, l->i);
	    memset(&key, 0, sizeof(key));
	    memset(&data, 0, sizeof(data));
	    key.data  = hello; key.size=strlen(hello)+1;
	    r=db->get(db, 0, &key, &data, 0);
	    assert(r==DB_NOTFOUND);
	}
    }
    r=db->close(db, 0);       assert(r==0);
    r=env->close(env, 0);     assert(r==0);
    while (items) {
	struct in_db *next=items->next;
	toku_free(items);
	items=next;
    }
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    make_db();
    return 0;
}
