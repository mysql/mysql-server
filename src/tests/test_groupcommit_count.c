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
#if 0
    r=env->txn_begin(env, 0, &tid, 0); CKERR(r);
    char there[1000];
    memset(there, 'a',sizeof(there));
    there[999]=0;
    for (i=0; sum<(effective_max*3)/2; i++) {
	DBT key,data;
	char hello[20];
	snprintf(hello, 20, "hello%d", i);
	r=db->put(db, tid,
		  dbt_init(&key, hello, strlen(hello)+1),
		  dbt_init(&data, there, sizeof(there)),
		  0);
	assert(r==0);
	sum+=strlen(hello)+1+sizeof(there);
	if ((i+1)%10==0) {
	    r=tid->commit(tid, 0); assert(r==0);
	    r=env->txn_begin(env, 0, &tid, 0); CKERR(r);
	}
    }
    if (verbose) printf("i=%d sum=%d effmax=%d\n", i, sum, effective_max);
    r=tid->commit(tid, 0); assert(r==0);
#endif

    r=db->close(db, 0); assert(r==0);
    r=env->close(env, 0); assert(r==0);

}

int fsync_count=0;
int do_fsync (int fd) {
    fsync_count++;
    return fsync(fd);
}

const char *progname;
struct timeval prevtime;
int prev_count;
void printtdiff (char *str) {
    struct timeval thistime;
    gettimeofday(&thistime, 0);
    printf("%s: %10.6fs %d fsyncs for %s\n", progname, thistime.tv_sec-prevtime.tv_sec+1e-6*(thistime.tv_usec-prevtime.tv_usec), fsync_count-prev_count, str);
    prevtime=thistime;
    prev_count=fsync_count;
}

int main (int argc, const char *argv[]) {
    progname=argv[0];
    parse_args(argc, argv);

    gettimeofday(&prevtime, 0);
    prev_count=0;

    { int r = db_env_set_func_fsync(do_fsync); CKERR(r); }

    system("rm -rf " ENVDIR);
    { int r=mkdir(ENVDIR, 0777);       assert(r==0); }

    test_groupcommit(1);  printtdiff("1 thread");
    test_groupcommit(2);  printtdiff("2 threads");
    test_groupcommit(10); printtdiff("10 threads");
    int count_before_20 = fsync_count;
    test_groupcommit(20); printtdiff("20 threads");
    if (fsync_count-count_before_20 >= 20*NITER) {
	printf("It looks like too many fsyncs.  Group commit doesn't appear to be occuring.\n");
	exit(1);
    }
    return 0;
}
