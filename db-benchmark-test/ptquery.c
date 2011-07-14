//#include <stdbool.h>
//#include "test.h"
//#include "toku_pthread.h"

#include <toku_portability.h>
#include "tokudb_common_funcs.h"
#include <toku_assert.h>
#include <db.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef TOKUDB
#include "key.h"
#include "cachetable.h"
#include "trace_mem.h"
#endif

DB_ENV *env;
DB *db;
DB_TXN *tid=0;

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)
static const char *dbdir = "./bench."  STRINGIFY(DIRSUF); /* DIRSUF is passed in as a -D argument to the compiler. */
static int env_open_flags_yesx = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOG|DB_INIT_LOCK|DB_RECOVER;
// static int env_open_flags_nox = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL;
static char *dbfilename = "bench.db";
static u_int64_t cachesize = 127*1024*1024;
static int verbose UU() = 0;
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
    r = db->open(db, tid, dbfilename, NULL, DB_BTREE, 0, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH); assert(r==0);
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
    int i;
    for (i=0; i<8 && i<array_size; i++)
	a[i] = (l>>(56-8*i))&0xff;
}

static void array_to_long_long (unsigned long long *l, unsigned char *a, int array_size) {
    int i;
    *l = 0;
    unsigned long long tmp;
    for(i=0; i<8 && i<array_size; i++) {
        tmp =  a[i] & 0xff;
        *l += tmp << (56-8*i);
    }
}

static void test_begin_commit(int nqueries) {
    int r;
    unsigned long long k;
    unsigned char kv[keylen];
    for (int i = 0; i < nqueries; i++) {
        DB_TXN *txn = NULL;
        r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);
        DBC *c = NULL;
        r = db->cursor(db, txn, &c, 0); assert_zero(r);

        k = (random() + (random() << 31)) % maxkey;
        k = ( k / SERIAL_SPACING ) * SERIAL_SPACING;
        long_long_to_array(kv, keylen, k);
        
        DBT key, val;
        memset(&key, 0, sizeof key); key.data=kv; key.size=8;
        memset(&val, 0, sizeof val);
        r = c->c_get(c, &key, &val, DB_SET); 
        assert_zero(r);
        (void) __sync_fetch_and_add(&set_count, 1);
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
    DBT key, val;

    double tstart = gettime();
    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);
    r = db->cursor(db, txn, &c, 0); assert_zero(r);
    r = c->c_get(c, &key, &val, DB_FIRST); assert_zero(r);
    assert(key.size == 8);
    while ( r != DB_NOTFOUND ) {
        r = c->c_get(c, &key, &val, DB_NEXT);
        if ( r != 0 && r != DB_NOTFOUND) assert_zero(r);
    }
    memset(&key, 0, sizeof key);
    memset(&val, 0, sizeof val);
    r = c->c_get(c, &key, &val, DB_LAST); assert_zero(r);
    array_to_long_long(&maxkey, key.data, key.size);
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


int test_main(int argc, char * const argv[]) {
    int nqueries = 1000000;
    int nthreads = 1;

    // parse_args(argc, argv);
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
