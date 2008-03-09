/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Test by counting the fsyncs, to see if group commit is working. */

#include <db.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "test.h"

DB_ENV *env;
DB *db;

#define NITER 100

void *start_a_thread (void *i_p) {
    int *which_thread_p = i_p;
    int i,r;
    for (i=0; i<NITER; i++) {
	DB_TXN *tid;
	char keystr[100];
	DBT key,data;
	snprintf(keystr, sizeof(key), "%ld.%d.%d", random(), *which_thread_p, i);
	r=env->txn_begin(env, 0, &tid, 0); CKERR(r);
	r=db->put(db, tid,
		  dbt_init(&key, keystr, 1+strlen(keystr)),
		  dbt_init(&data, keystr, 1+strlen(keystr)),
		  0);
	r=tid->commit(tid, 0); CKERR(r);
    }
    return 0;
}

void test_groupcommit (int nthreads) {
    int r;
    DB_TXN *tid;

    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_THREAD, 0777); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
    r=tid->commit(tid, 0);    assert(r==0);

    int i;
    pthread_t threads[nthreads];
    int whichthread[nthreads];
    for (i=0; i<nthreads; i++) {
	whichthread[i]=i;
	r=pthread_create(&threads[i], 0, start_a_thread, &whichthread[i]);
    }
    for (i=0; i<nthreads; i++) {
	pthread_join(threads[i], 0);
    }

    r=db->close(db, 0); assert(r==0);
    r=env->close(env, 0); assert(r==0);

}

#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
static pthread_mutex_t fsync_count_lock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#else
static pthread_mutex_t fsync_count_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

// helgrind doesn't understand that pthread_join removes a race condition.   I'm not impressed... -Bradley
// Also, it doesn't happen every time, making helgrind unsuitable for regression tests.
// So we must put locks around things that are properly serialized anyway.

int fsync_count_lockprotected=0;
void inc_fsync_count(void) {
    int r;
    r=pthread_mutex_lock(&fsync_count_lock); assert(r==0);
    fsync_count_lockprotected++;
    pthread_mutex_unlock(&fsync_count_lock); assert(r==0);
}

int get_fsync_count(void) {
    int r;
    r=pthread_mutex_lock(&fsync_count_lock); assert(r==0);
    int result=fsync_count_lockprotected;
    pthread_mutex_unlock(&fsync_count_lock); assert(r==0);
    return result;
}

int do_fsync (int fd) {
    inc_fsync_count();
    return fsync(fd);
}

const char *progname;
struct timeval prevtime;
int prev_count;
void printtdiff (char *str) {
    struct timeval thistime;
    gettimeofday(&thistime, 0);
    double tdiff = thistime.tv_sec-prevtime.tv_sec+1e-6*(thistime.tv_usec-prevtime.tv_usec);
    int fcount=get_fsync_count();
    printf("%s: %10.6fs %d fsyncs for %s\n", progname, tdiff, fcount-prev_count, str);
    prevtime=thistime;
    prev_count=fcount;
}

int main (int argc, const char *argv[]) {
    progname=argv[0];
    parse_args(argc, argv);

    printf("&fsync_count=%p\n", &fsync_count_lockprotected);

    gettimeofday(&prevtime, 0);
    prev_count=0;

    { int r = db_env_set_func_fsync(do_fsync); CKERR(r); }

    system("rm -rf " ENVDIR);
    { int r=mkdir(ENVDIR, 0777);       assert(r==0); }

    test_groupcommit(1);  printtdiff("1 thread");
    test_groupcommit(2);  printtdiff("2 threads");
    int count_before_10 = get_fsync_count();
    test_groupcommit(10); printtdiff("10 threads");
    if (get_fsync_count()-count_before_10 >= 10*NITER) {
	printf("It looks like too many fsyncs.  Group commit doesn't appear to be occuring.\n");
	exit(1);
    }
    if (0) { // valgrind --tool=helgrind cannot handle 20 threads.
	int count_before_20 = get_fsync_count();
	test_groupcommit(20); printtdiff("20 threads");
	if (get_fsync_count()-count_before_20 >= 20*NITER) {
	    printf("It looks like too many fsyncs.  Group commit doesn't appear to be occuring.\n");
	    exit(1);
	}
    }
    return 0;
}
