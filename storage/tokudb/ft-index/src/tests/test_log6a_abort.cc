/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#include "test.h"

/* Like test_log6 except abort.
 * And abort some stuff, but not others (unlike test_log6_abort which aborts everything) */


#include <db.h>
#include <stdlib.h>
#include <search.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <memory.h>


#ifndef DB_DELETE_ANY
#define DB_DELETE_ANY 0 
#endif

// TOKU_TEST_FILENAME is defined in the Makefile

// How many iterations are we going to do insertions and deletions.  This is a bound to the number of distinct keys in the DB.
#define N 1000

static int n_keys_mentioned=0;
static int random_keys_mentioned[N];

static DB *pending_i, *pending_d, *committed;

// Keep track of what's in the committed database separately
struct pair {int x,y;};

static void
insert_in_mem (int x, int y, int *count, struct pair *pairs) {
    assert(*count<N);
    pairs[(*count)++]=(struct pair){x,y};
}
static void
delete_in_mem (int x, int *count, struct pair *pairs) {
    int i;
    for (i=0; i<*count; i++) {
	if (pairs[i].x==x) {
	    pairs[i]=pairs[--(*count)];
	    return;
	}
    }
}

static int         com_count=0, pend_count=0, peni_count=0;
static struct pair com_data[N], pend_data[N], peni_data[N];

static void
insert_pending (int key, int val, DB_TXN *bookx) {
    DBT keyd,datad;
    //printf("IP %u,%u\n", key,val);

    insert_in_mem(key, val, &peni_count, peni_data);
    pending_i->put(pending_i, bookx,
		   dbt_init(&keyd, &key, sizeof(key)),
		   dbt_init(&datad, &val, sizeof(val)),
		   0);

    delete_in_mem(key, &pend_count, pend_data);
    pending_d->del(pending_d, bookx,
		   dbt_init(&keyd, &key, sizeof(key)),
		   0);
}

static void put_a_random_item (DB *db, DB_TXN *tid, int i, DB_TXN *bookx) {
    char hello[30], there[30];
    DBT key,data;
    int randv = myrandom();
    random_keys_mentioned[n_keys_mentioned++] = randv;
    insert_pending(randv, i, bookx);
    //printf("Insert %u\n", randv);
    snprintf(hello, sizeof(hello), "hello%d.%d", randv, i);
    snprintf(there, sizeof(hello), "there%d", i);
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data  = hello; key.size=strlen(hello)+1;
    data.data = there; data.size=strlen(there)+1;
    int r=db->put(db, tid, &key, &data, 0);
    if (r!=0) printf("%s:%d i=%d r=%d (%s)\n", __FILE__, __LINE__, i, r, strerror(r));
    assert(r==0);
}

static void delete_a_random_item (DB *db, DB_TXN *tid, DB_TXN *bookx) { 
    if (n_keys_mentioned==0) return;
    int ridx = myrandom()%n_keys_mentioned;
    int randv = random_keys_mentioned[ridx];
    DBT keyd;
    DBT vald;
    //printf("Delete %u\n", randv);
    dbt_init(&keyd, &randv, sizeof(randv));
    dbt_init(&vald, &randv, sizeof(randv));

    pending_i->del(pending_i, bookx, &keyd, 0);
    delete_in_mem(randv, &peni_count, peni_data);

    pending_d->put(pending_d, bookx, &keyd, &vald, 0);
    insert_in_mem(randv, randv, &pend_count, pend_data);

    db->del(db, tid, &keyd, DB_DELETE_ANY);
}

static void commit_items (DB_ENV *env, int UU(i)) {
    //printf("commit_items %d\n", i);
    DB_TXN *txn;
    int r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    DBC  *cursor;
    r = pending_i->cursor(pending_i, txn, &cursor, 0); assert(r==0);
    DBT k,v;
    memset(&k,0,sizeof(k));
    memset(&v,0,sizeof(v));
    //printf("%d items in peni\n", peni_count);
    while (cursor->c_get(cursor, &k, &v, DB_FIRST)==0) {
	assert(k.size==4);
	assert(v.size==4);
	int ki=*(int*)k.data;
	int vi=*(int*)v.data;
	//printf(" put %u %u\n", ki, vi);
	r=committed->put(committed, txn, dbt_init(&k, &ki, sizeof(ki)), dbt_init(&v, &vi, sizeof(vi)), 0);
	insert_in_mem(ki, vi, &com_count, com_data);
	assert(r==0);
	r=pending_i->del(pending_i, txn, &k, 0);
	assert(r==0);
    }
    r=cursor->c_close(cursor);
    assert(r==0);

    r = pending_d->cursor(pending_d, txn, &cursor, 0); assert(r==0);
    memset(&k,0,sizeof(k));
    memset(&v,0,sizeof(v));
    while (cursor->c_get(cursor, &k, &v, DB_FIRST)==0) {
	assert(k.size==4);
	assert(v.size==4);
	int ki=*(int*)k.data;
	int vi=*(int*)v.data;
	assert(ki==vi);
	//printf(" del %u\n", ki);
	committed->del(committed, txn, dbt_init(&k, &ki, sizeof(ki)), 0);
	delete_in_mem(ki, &com_count, com_data);
	// ignore result from that del
	r=pending_d->del(pending_d, txn, &k, 0);
	assert(r==0);
    }
    r=cursor->c_close(cursor);
    assert(r==0);
    r=txn->commit(txn, 0); assert(r==0);
}

static void abort_items (DB_ENV *env) {
    DB_TXN *txn;
    int r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    //printf("abort_items\n");
    DBC  *cursor;
    r = pending_i->cursor(pending_i, txn, &cursor, 0); assert(r==0);
    DBT k,v;
    memset(&k,0,sizeof(k));
    memset(&v,0,sizeof(v));
    while (cursor->c_get(cursor, &k, &v, DB_FIRST)==0) {
	assert(k.size==4);
	assert(v.size==4);
	int ki=*(int*)k.data;
	//printf("Deleting %u\n", ki);
	r=pending_i->del(pending_i, txn, dbt_init(&k, &ki, sizeof(ki)), 0);
	assert(r==0);
    }
    r=cursor->c_close(cursor);
    assert(r==0);

    r = pending_d->cursor(pending_d, txn, &cursor, 0); assert(r==0);
    memset(&k,0,sizeof(k));
    memset(&v,0,sizeof(v));
    while (cursor->c_get(cursor, &k, &v, DB_FIRST)==0) {
	assert(k.size==4);
	assert(v.size==4);
	int ki=*(int*)k.data;
	r=pending_d->del(pending_d, txn, dbt_init(&k, &ki, sizeof(ki)), 0);
	assert(r==0);
    }
    r=cursor->c_close(cursor);
    assert(r==0);
    r=txn->commit(txn, 0); assert(r==0);
}

static int
compare_pairs (const void *a, const void *b) {
    return memcmp(a,b,4);
}

static void verify_items (DB_ENV *env, DB *db) {
    DB_TXN *txn;
    int r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    DBC *cursor;
    DBT k,v;
    memset(&k,0,sizeof(k));
    memset(&v,0,sizeof(v));

#if 0
    r=db->cursor(db, txn, &cursor, 0);
    assert(r==0);
    while (cursor->c_get(cursor, &k, &v, DB_NEXT)==0) {
    }
    r=cursor->c_close(cursor);
    assert(r==0);
#endif

    r = committed->cursor(committed, txn, &cursor, 0);
    assert(r==0);
    qsort(com_data, com_count, sizeof(com_data[0]), compare_pairs);
    int curscount=0;
    //printf(" count=%d\n", com_count);
    while (cursor->c_get(cursor, &k, &v, DB_NEXT)==0) {
	int kv=*(int*)k.data;
	int dv=*(int*)v.data;
	//printf(" sorted com_data[%d]=%d, cursor got %d\n", curscount, com_data[curscount].x, kv);
	assert(com_data[curscount].x==kv);
	DBT k2,v2;
	memset(&k2, 0, sizeof(k2));
	memset(&v2, 0, sizeof(v2));
	char hello[30], there[30];
	snprintf(hello, sizeof(hello), "hello%d.%d", kv, dv);
	snprintf(there, sizeof(hello), "there%d", dv);
	k2.data  = hello; k2.size=strlen(hello)+1;
	//printf("committed: %u,%u\n", kv, dv);
	r=db->get(db, txn,  &k2, &v2, 0);
	assert(r==0);
	assert(strcmp((char*)v2.data, there)==0);
	curscount++;
    }
    assert(curscount==com_count);
    r=cursor->c_close(cursor);
    assert(r==0);

    r=txn->commit(txn, 0); assert(r==0);
}

static void make_db (void) {
    DB_ENV *env;
    DB *db;
    DB_TXN *tid, *bookx;
    int r;
    int i;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=db_create(&pending_i, env, 0); CKERR(r);
    r=db_create(&pending_d, env, 0); CKERR(r);
    r=db_create(&committed, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=pending_i->open(pending_i, tid, "pending_i.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=pending_d->open(pending_d, tid, "pending_d.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=committed->open(committed, tid, "committed.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=tid->commit(tid, 0);    assert(r==0);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=env->txn_begin(env, 0, &bookx, 0); assert(r==0);

    for (i=0; i<N; i++) {
	int randv = myrandom();
	//if (i%10000==0) printf(".");
	if (randv%100==0) {
	    r=tid->abort(tid); assert(r==0);
	    r=bookx->commit(bookx, 0); assert(r==0);
	    r=env->txn_begin(env, 0, &bookx, 0); assert(r==0);
	    abort_items(env);
	    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
	} else if (randv%1000==1) {
	    r=tid->commit(tid, 0); assert(r==0);
	    r=bookx->commit(bookx, 0); assert(r==0);
	    r=env->txn_begin(env, 0, &bookx, 0); assert(r==0);
	    commit_items(env, i);
	    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
	} else if (randv%3==0) {
	    delete_a_random_item(db, tid, bookx);
	} else {
	    put_a_random_item(db, tid, i, bookx);
	}
    }
    r=tid->commit(tid, 0); assert(r==0);
    r=bookx->commit(bookx, 0); assert(r==0);
    commit_items(env, i);
    verify_items(env, db);

    r=pending_i->close(pending_i, 0); assert(r==0);
    r=pending_d->close(pending_d, 0); assert(r==0);
    r=committed->close(committed, 0); assert(r==0);
    r=db->close(db, 0);       assert(r==0);
    r=env->close(env, 0);     assert(r==0);
}

int
test_main (int argc __attribute__((__unused__)), char *const argv[] __attribute__((__unused__))) {
    make_db();
    return 0;
}
