/* This is a performance test.  Releasing lock during I/O should mean that given two threads doing queries,
 * and one of them is in-memory and one of them is out of memory, then the in-memory one should not be slowed down by the out-of-memory one.
 * 
 * Step 1: Create a dictionary that doesn't fit in main memory.  Do it fast (sequential insertions).
 * Step 2: Measure performance of in-memory requests.
 * Step 3: Add a thread that does requests in parallel.
 */

#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include <string.h>
#include <toku_time.h>
#include <toku_pthread.h>

static const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

#define ROWSIZE 100
static const char dbname[] = "data.db";
static unsigned long long n_rows;

static DB_ENV *env = NULL;
static DB *db;

// BDB cannot handle big transactions  by default (runs out of locks).
#ifdef TOKUDB
#define N_PER_XACTION 10000
#else
#define N_PER_XACTION 1000
#endif

static void create_db (u_int64_t N) {
    n_rows = N;
    { int r = system("rm -rf " ENVDIR);                                        CKERR(r); }
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    { int r = db_env_create(&env, 0);                                          CKERR(r); }
    env->set_errfile(env, stderr);
#ifdef TOKUDB
    env->set_redzone(env, 0);
#endif
    { int r = env->set_cachesize(env, 0, 400*4096, 1);                        CKERR(r); }
    { int r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r); }
    DB_TXN *txn;
    { int r = env->txn_begin(env, NULL, &txn, 0);                              CKERR(r); }
    { int r = db_create(&db, env, 0);                                          CKERR(r); }
    { int r = db->set_pagesize(db, 4096);                                      CKERR(r); }
    { int r = db->open(db, txn, dbname, NULL, DB_BTREE, DB_CREATE, 0666);      CKERR(r); }
    { int r = txn->commit(txn, DB_TXN_NOSYNC);                                 CKERR(r); }

    { int r = env->txn_begin(env, NULL, &txn, 0);                              CKERR(r); }
    u_int64_t n_since_commit = 0;
    for (unsigned long long i=0; i<N; i++) {
	if (n_since_commit++ > N_PER_XACTION) {
	    { int r = txn->commit(txn, DB_TXN_NOSYNC);                         CKERR(r); }
	    { int r = env->txn_begin(env, NULL, &txn, 0);                      CKERR(r); }
	}
	char key[20];
	char data[200];
	snprintf(key,  sizeof(key),  "%016llx", i);
	snprintf(data, sizeof(data), "%08lx%08lx%66s", random(), random()%16, "");
	DBT keyd, datad;
	{
	    int r = db->put(db, txn, dbt_init(&keyd, key, strlen(key)+1), dbt_init(&datad, data, strlen(data)+1), 0);
	    CKERR(r);
	}
    }
    //printf("n_rows=%lld\n", n_rows);
    { int r = txn->commit(txn, DB_TXN_NOSYNC);                                 CKERR(r); }
}

struct reader_thread_state {
    /* output */
    double             elapsed_time;
    unsigned long long n_did_read;

    /* input */
    signed long long n_to_read;  // Negative if we just run forever
    int              do_local;

    /* communicate to the thread while running */
    volatile int finish;

};

static
void* reader_thread (void *arg)
// Return the time to read
{
    struct timeval start_time, end_time;
    gettimeofday(&start_time, 0);

    DB_TXN *txn;
    struct reader_thread_state *rs = (struct reader_thread_state *)arg;
    
    { int r = env->txn_begin(env, NULL, &txn, 0);                              CKERR(r); }
    char key[20];
    char data [200];
    DBT keyd  = { .data = key,  .size = 0, .ulen = sizeof(key),  .flags = DB_DBT_USERMEM };
    DBT datad = { .data = data, .size = 0, .ulen = sizeof(data), .flags = DB_DBT_USERMEM };

#define N_DISTINCT 16
    unsigned long long vals[N_DISTINCT];
    if (rs->do_local) {
	for (int i=0; i<N_DISTINCT; i++) {
	    vals[i] = random()%n_rows;
	}
    }
    
    u_int64_t n_since_commit = 0;
    long long n_read_so_far = 0;
    while ((!rs->finish) && ((rs->n_to_read < 0) || (n_read_so_far < rs->n_to_read))) {

	if (n_since_commit++ > N_PER_XACTION) {
	    { int r = txn->commit(txn, DB_TXN_NOSYNC);                         CKERR(r); }
	    { int r = env->txn_begin(env, NULL, &txn, 0);                      CKERR(r); }
	    n_since_commit = 0;
	}
	long long value;
	if (rs->do_local) {
	    long which = random()%N_DISTINCT;
	    value = vals[which];
	    //printf("value=%lld\n", value);
	} else {
	    value = random()%n_rows;
	}
	snprintf(key,  sizeof(key),  "%016llx", value);
	keyd.size = strlen(key)+1;
	int r = db->get(db, txn, &keyd, &datad, 0);
	CKERR(r);
	rs->n_did_read++;
	n_read_so_far ++;
    }
    { int r = txn->commit(txn, DB_TXN_NOSYNC);                                 CKERR(r); }
    
    gettimeofday(&end_time, 0);
    rs->elapsed_time = toku_tdiff(&end_time, &start_time);
    return NULL;
}

static
void do_threads (unsigned long long N, int do_nonlocal) {
    toku_pthread_t ths[2];
    struct reader_thread_state rstates[2] = {{.n_to_read = N,
					      .do_local  = 1,
					      .finish    = 0},
					     {.n_to_read = -1,
					      .do_local  = 0,
					      .finish    = 0}};
    int n_to_create = do_nonlocal ? 2 : 1;
    for (int i=0; i<n_to_create; i++) {
	int r =  toku_pthread_create(&ths[i], 0, reader_thread, (void*)&rstates[i]);
	CKERR(r);
    }
    for (int i=0; i<n_to_create; i++) {
	void *retval;
	int r = toku_pthread_join(ths[i], &retval);
	CKERR(r);
	assert(retval==0);
	if (verbose) {
	    printf("%9s thread time = %8.2fs on %9lld reads (%.3f us/read)\n",
		   (i==0 ? "local" : "nonlocal"),
		   rstates[i].elapsed_time, rstates[i].n_did_read, rstates[i].elapsed_time/rstates[i].n_did_read * 1e6);
	}
	rstates[1].finish = 1;
    }
    if (verbose && do_nonlocal) {
	printf("total                                %9lld reads (%.3f us/read)\n",
	       rstates[0].n_did_read + rstates[1].n_did_read,
	       (rstates[0].elapsed_time)/(rstates[0].n_did_read + rstates[1].n_did_read) * 1e6);
    }
}

static volatile unsigned long long n_preads;

static ssize_t my_pread (int fd, void *buf, size_t count, off_t offset) {
    (void) __sync_fetch_and_add(&n_preads, 1);
    usleep(1000); // sleep for a millisecond
    return pread(fd, buf, count, offset);
}

unsigned long N_default = 100000;
unsigned long N;

static void my_parse_args (int argc, char * const argv[]) {
    const char *progname = argv[0];
    argc--; argv++;
    verbose = 0;
    N = N_default;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    if (verbose>0) verbose--;
	} else if (strcmp(argv[0],"-n")==0) {
	    argc--; argv++;
	    if (argc==0) goto usage;
	    errno = 0; 
	    char *end;
	    N = strtol(argv[0], &end, 10);
	    if (errno!=0 || *end!=0) goto usage;
	} else {
	usage:
	    fprintf(stderr, "Usage:\n %s [-v] [-q] [-n <rowcount> (default %ld)]\n", progname, N_default);
	    fprintf(stderr, "  -n 10000     is probably good for valgrind.\n");
	    exit(1);
	}
	argc--; argv++;
    }

}

int test_main (int argc, char * const argv[])  {
    my_parse_args(argc, argv);

    unsigned long long M = N*10;

    db_env_set_func_pread(my_pread);

    create_db (N);
    if (verbose) printf("%lld preads\n", n_preads);
    do_threads (M, 0);
    if (verbose) printf("%lld preads\n", n_preads);
    do_threads (M, 0);
    if (verbose) printf("%lld preads\n", n_preads);
    do_threads (M, 1);
    if (verbose) printf("%lld preads\n", n_preads);
    { int r = db->close(db, 0);                                                CKERR(r); }
    { int r = env->close(env, 0);                                              CKERR(r); }
    if (verbose) printf("%lld preads\n", n_preads);
    return 0;
}

