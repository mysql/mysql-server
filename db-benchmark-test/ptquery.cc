/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <toku_portability.h>
#include "tokudb_common_funcs.h"
#include <toku_pthread.h>
#include <toku_assert.h>
#include <portability/toku_atomic.h>
#include <db.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static DB_ENV *env = NULL;
static DB *db = NULL;

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)
static const char *dbdir = "./bench."  STRINGIFY(DIRSUF); /* DIRSUF is passed in as a -D argument to the compiler. */
static int env_open_flags_yesx = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOG|DB_INIT_LOCK|DB_RECOVER|DB_THREAD;
// static int env_open_flags_nox = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL;
static const char *dbfilename = "bench.db";
static uint64_t cachesize = 127*1024*1024;
static int nqueries = 1000000;
static int nthreads = 1;
static const char *log_dir = NULL;

static long long set_count = 0;

static void pt_query_setup (void) {
    int r;
    r = db_env_create(&env, 0);                                                           assert(r==0);
    r = env->set_cachesize(env, cachesize / (1<<30), cachesize % (1<<30), 1);             assert(r==0);
    if (log_dir) {
        r = env->set_lg_dir(env, log_dir);                                                assert(r==0);
    }
    r = env->open(env, dbdir, env_open_flags_yesx, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);       assert(r==0);
    r = db_create(&db, env, 0);                                                            assert(r==0);
    r = db->open(db, NULL, dbfilename, NULL, DB_BTREE, DB_THREAD|DB_AUTO_COMMIT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH); assert(r==0);
}

static void pt_query_shutdown (void) {
    int r;
    r = db->close(db, 0);                                       assert(r==0);
    r = env->close(env, 0);                                     assert(r==0);
    env = NULL;
}

static unsigned long long maxkey;
enum { SERIAL_SPACING = 1<<6 };
static const int keylen=8;

static double gettime (void) {
    struct timeval tv;
    int r = gettimeofday(&tv, 0);
    assert(r==0);
    return tv.tv_sec + 1e-6*tv.tv_usec;
}

/* From db-benchmark-test.c */
static void long_long_to_array (unsigned char *a, int array_size, unsigned long long l) {
    for (int i=0; i<8 && i<array_size; i++)
	a[i] = (l>>(56-8*i))&0xff;
}

static void array_to_long_long (unsigned long long *l, unsigned char *a, int array_size) {
    *l = 0;
    for(int i=0; i<8 && i<array_size; i++) {
        unsigned long long tmp =  a[i] & 0xff;
        *l += tmp << (56-8*i);
    }
}

#if TOKUDB
static int do_nothing (DBT const* UU(key), DBT const* UU(data), void* UU(extrav)) {
    return TOKUDB_CURSOR_CONTINUE;
}
#endif

static void test_begin_commit(int _nqueries) {
    int r;
    unsigned long long k;
    unsigned char kv[keylen];
    for (int i = 0; i < _nqueries; i++) {
        DB_TXN *txn = NULL;
        r = env->txn_begin(env, NULL, &txn, DB_TXN_SNAPSHOT); assert_zero(r);
        DBC *c = NULL;
        r = db->cursor(db, txn, &c, 0); assert_zero(r);

        k = (random() + (random() << 31)) % maxkey;
        k = ( k / SERIAL_SPACING ) * SERIAL_SPACING;
        long_long_to_array(kv, keylen, k);
        
        DBT key = { .data = kv, .size = 8 };
        DBT val = { .data = NULL, .size = 0 };
#if TOKUDB
        r = c->c_getf_set(c, 0, &key, do_nothing, &val);
#else
        r = c->c_get(c, &key, &val, DB_SET);
#endif
        assert_zero(r);
        (void) toku_sync_fetch_and_add(&set_count, 1);
        r = c->c_close(c); assert_zero(r);
        r = txn->commit(txn, 0); assert_zero(r);
    }
}


// read in the entire DB, warming up the cache
//  - record maxkey 
static void warmup(void) {
    int r;
    DB_TXN *txn=NULL;
    DBC *c = NULL;
    DBT key = { .data = NULL };
    DBT val = { .data = NULL };
    double tstart = gettime();
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);
    r = db->cursor(db, txn, &c, 0); assert_zero(r);
    r = c->c_get(c, &key, &val, DB_FIRST); assert_zero(r);
    assert(key.size == 8);
    while ( r != DB_NOTFOUND ) {
#if TOKUDB
        r = c->c_getf_next(c, DB_PRELOCKED, do_nothing, NULL);
#else
        r = c->c_get(c, &key, &val, DB_NEXT);
#endif
        if ( r != 0 && r != DB_NOTFOUND) assert_zero(r);
    }
    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    r = c->c_get(c, &key, &val, DB_LAST); assert_zero(r);
    array_to_long_long(&maxkey, (unsigned char*)key.data, key.size);
    r = c->c_close(c); assert_zero(r);
    r = txn->commit(txn, 0); assert_zero(r);
    double tdiff = gettime() - tstart;
    unsigned long long rows = maxkey / SERIAL_SPACING;
    printf("Warmup        : read %12llu rows in %6.1fs %8.0f/s\n", rows, tdiff, rows/tdiff); fflush(stdout);
}


struct arg {
//    DB_ENV *env;
//    DB *db;
    int nqueries;
//    int nrows;
};

static void *test_thread(void *arg) {
    struct arg *myarg = (struct arg *) arg;
    test_begin_commit(myarg->nqueries);
    return arg;
}

static void usage(void) {
    fprintf(stderr, "--nqueries %d\n", nqueries);
    fprintf(stderr, "--nthreads %d\n", nthreads);
    fprintf(stderr, "--cachesize %" PRId64 "\n", cachesize);
}

int test_main(int argc, char * const argv[]) {
    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "--nqueries") == 0 && i+1 < argc) {
            nqueries = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--nthreads") == 0 && i+1 < argc) {
            nthreads = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--cachesize") == 0 && i+1 < argc) {
            cachesize = atoll(argv[++i]);
            continue;
        }
        usage();
        return 1;
    }

    int r;
    pt_query_setup();

    warmup();
    
    assert(nthreads > 0);
    struct arg myargs[nthreads-1];
    toku_pthread_t mytids[nthreads];
    double tstart = gettime();
    for (int i = 0; i < nthreads-1; i++) {
        myargs[i] = (struct arg) { nqueries };
        r = toku_pthread_create(&mytids[i], NULL, test_thread, &myargs[i]); assert_zero(r);
    }
    test_begin_commit(nqueries);
    for (int i = 0; i < nthreads-1; i++) {
        void *ret;
        r = toku_pthread_join(mytids[i], &ret); assert_zero(r);
    }
    assert(set_count == nthreads * nqueries);
    double tdiff = gettime() - tstart;
    printf("Point Queries : read %12llu rows in %6.1fs %8.0f/s with %d threads\n", set_count, tdiff, set_count/tdiff, nthreads); fflush(stdout);

    pt_query_shutdown();
    return 0;
}
