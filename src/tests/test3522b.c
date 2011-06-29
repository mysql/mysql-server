/* -*- mode: C; c-basic-offset: 4 -*- */

/* Test for #3522.    Demonstrate that with DB_TRYAGAIN a cursor can stall.
 * Strategy: Create a tree (with relatively small nodes so things happen quickly, and relatively large compared to the cache).
 *  In a single transaction: Delete everything except the last one, and then do a DB_FIRST.
 *    (Compare to test3522.c which deletes everything including the last one.)
 *  Make the test terminate by capturing the calls to pread(). */

#ident "$Id$"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

static DB_ENV *env;
static DB *db;
const int N = 1000;

const int n_preads_limit = 1000;
long n_preads = 0;

static ssize_t my_pread (int fd, void *buf, size_t count, off_t offset) {
    long n_read_so_far = __sync_fetch_and_add(&n_preads, 1);
    if (n_read_so_far > n_preads_limit) {
	if (verbose) fprintf(stderr, "Apparent infinite loop detected\n");
	abort();
    }
    return pread(fd, buf, count, offset);
}

static void
insert(int i, DB_TXN *txn)
{
    char hello[30], there[30];
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(there, sizeof(there), "there%d", i);
    DBT key, val;
    int r=db->put(db, txn,
		  dbt_init(&key, hello, strlen(hello)+1),
		  dbt_init(&val, there, strlen(there)+1),
		  0);
    CKERR(r);
}

static void delete (int i, DB_TXN *x) {
    char hello[30];
    DBT key;
    if (verbose>1) printf("delete %d\n", i);
    snprintf(hello, sizeof(hello), "hello%d", i);
    int r = db->del(db, x,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    0);
    CKERR(r);
}

static void
setup (void) {
    db_env_set_func_pread(my_pread);
    int r;
    r = system("rm -rf " ENVDIR);                                                     CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create(&env, 0);                                                       CKERR(r);
    r = env->set_redzone(env, 0);                                                     CKERR(r);
    r = env->set_cachesize(env, 0, 128*1024, 1);                                      CKERR(r);
    r = env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_create(&db, env, 0);                                                       CKERR(r);
    r = db->set_pagesize(db, 4096);                                                   CKERR(r);
    {
	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
	r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
	r = txn->commit(txn, 0);                                                          CKERR(r);
    }
    {
	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
	for (int i=0; i<N; i++) insert(i, txn);
	r = txn->commit(txn, 0);                                                          CKERR(r);
    }
}

static void finish (void) {
    int r;
    r = db->close(db, 0);                                                             CKERR(r);
    r = env->close(env, 0);                                                           CKERR(r);
}


int did_nothing_count = 0;
int expect_n = -1;

static int
do_nothing(DBT const *a, DBT  const *b, void *c) {
    did_nothing_count++;
    assert(c==NULL);
    char hello[30], there[30];
    snprintf(hello, sizeof(hello), "hello%d", expect_n);
    snprintf(there, sizeof(there), "there%d", expect_n);
    assert(strlen(hello)+1 == a->size);
    assert(strlen(there)+1 == b->size);
    assert(strcmp(hello, a->data)==0);
    assert(strcmp(there, b->data)==0);
    return 0;
}
static void run_del_next (void) {
    DB_TXN *txn;
    DBC *cursor;
    int r;
    r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
    for (int i=0; i<N-1; i++) delete(i, txn);
    r = db->cursor(db, txn, &cursor, 0);                                              CKERR(r);
    expect_n = N-1;
    did_nothing_count = 0;
    n_preads = 0;
    if (verbose) printf("read_next\n");
    r = cursor->c_getf_next(cursor, 0, do_nothing, NULL);                             CKERR(r);
    assert(did_nothing_count==1);
    if (verbose) printf("n_preads=%ld\n", n_preads);
    r = cursor->c_close(cursor);                                                      CKERR(r);
    r = txn->commit(txn, 0);                                                          CKERR(r);
}

static void run_del_prev (void) {
    DB_TXN *txn;
    DBC *cursor;
    int r;
    r = env->txn_begin(env, 0, &txn, 0);                                              CKERR(r);
    for (int i=1; i<N; i++) delete(i, txn);
    r = db->cursor(db, txn, &cursor, 0);                                              CKERR(r);
    expect_n = 0;
    did_nothing_count = 0;
    if (verbose) printf("read_prev\n");
    n_preads = 0;
    r = cursor->c_getf_prev(cursor, 0, do_nothing, NULL);                             CKERR(r);
    assert(did_nothing_count==1);
    if (verbose) printf("n_preads=%ld\n", n_preads);
    r = cursor->c_close(cursor);                                                      CKERR(r);
    r = txn->commit(txn, 0);                                                          CKERR(r);
}

static void run_test (void) {
    setup();
    run_del_next();
    finish();

    setup();
    run_del_prev();
    finish();
}
int test_main (int argc, char*const argv[]) {
    parse_args(argc, argv);
    run_test();
    return 0;
}


