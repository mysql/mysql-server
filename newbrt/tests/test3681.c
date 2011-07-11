/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."
#ident "$Id$"

// Test for #3681: iibench hangs.  The scenario is
//  * Thread 1 calls root_put_cmd, get_and_pin_root, 1 holds read lock on the root.
//  * Thread 2 calls checkpoint, marks the root for checkpoint.
//  * Thread 2 calls end_checkpoint, tries to write lock the root, sets want_write, and blocks on the rwlock because there is a reader.
//  * Thread 1 calls apply_cmd_to_in_memory_leaves, calls get_and_pin_if_in_memory, tries to get a read lock on the root node and blocks on the rwlock because there is a write request on the lock.

#include "includes.h"
#include "checkpoint.h"
#include "test.h"

CACHETABLE ct;
BRT t;

static DB * const null_db = 0;
static TOKUTXN const null_txn = 0;

volatile bool done = false;

static void setup (void) {
    { int r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);                                  assert(r==0); }
    char fname[] = __FILE__ "test1.dat";
    unlink(fname);
    { int r = toku_open_brt(fname, 1, &t, 1024, ct, null_txn, toku_builtin_compare_fun, null_db);         assert(r==0); }
}


static void finish (void) {
    { int r = toku_close_brt(t, 0);                                                                       assert(r==0); };
    { int r = toku_cachetable_close(&ct);                                                    assert(r == 0 && ct == 0); }
}

static void *starta (void *n) {
    assert(n==NULL);
    for (int i=0; i<10000; i++) {
	DBT k,v;
	char ks[20], vs[20];
	snprintf(ks, sizeof(ks), "hello%03d", i);
	snprintf(vs, sizeof(vs), "there%03d", i);
	int r = toku_brt_insert(t, toku_fill_dbt(&k, ks, strlen(ks)), toku_fill_dbt(&v, vs, strlen(vs)), null_txn);
	assert(r==0);
	usleep(1);
    }
    done = true;
    return NULL;
}
static void *startb (void *n) {
    assert(n==NULL);
    int count=0;
    while (!done) {
	int r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL); assert(r==0);
	count++;
    }
    printf("count=%d\n", count);
    return NULL;
}

static void test3681 (void) {
    setup();
    toku_pthread_t a,b;
    { int r; r = toku_pthread_create(&a, NULL, starta, NULL); assert(r==0); }
    { int r; r = toku_pthread_create(&b, NULL, startb, NULL); assert(r==0); }
    { int r; void *v; r = toku_pthread_join(a, &v);           assert(r==0); assert(v==NULL); }
    { int r; void *v; r = toku_pthread_join(b, &v);           assert(r==0); assert(v==NULL);  }
    finish();
}

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test3681();
    return 0;
}

