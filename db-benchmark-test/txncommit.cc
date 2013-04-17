/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// run txn begin commit on multiple threads and measure the throughput

#include "toku_portability.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include "db.h"
#include "toku_pthread.h"

static double now(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL); assert(r == 0);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

static void test_txn(DB_ENV *env, uint32_t ntxns, long usleep_time) {
    int r;
    for (uint32_t i = 0; i < ntxns; i++) {
        DB_TXN *txn = NULL;
        r = env->txn_begin(env, NULL, &txn, DB_TXN_SNAPSHOT); assert(r == 0);
        r = txn->commit(txn, 0); assert(r == 0);
        if (usleep_time)
            usleep(usleep_time); 
        else
            sched_yield();
    }
}

struct arg {
    DB_ENV *env;
    uint32_t ntxns;
    long usleep_time;
};

static void *test_thread(void *arg) {
    struct arg *myarg = (struct arg *) arg;
    test_txn(myarg->env, myarg->ntxns, myarg->usleep_time);
    return arg;
}

int main(int argc, char * const argv[]) {
    uint32_t ntxns = 1000000;
    uint32_t nthreads = 1;
    long usleep_time = 0;

    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "--ntxns") == 0 && i+1 < argc) {
            ntxns = atoll(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--nthreads") == 0 && i+1 < argc) {
            nthreads = atoll(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--usleep") == 0 && i+1 < argc) {
            usleep_time = atol(argv[++i]);
            continue;
        }
        return 1;
    }

    int r;

    char cmd[256];
    sprintf(cmd, "rm -rf %s", ENVDIR);
    r = system(cmd); assert(r == 0);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_cachesize(env, 4, 0, 1); assert(r == 0);
    r = env->open(env, ENVDIR, DB_PRIVATE + DB_THREAD + DB_CREATE + DB_INIT_TXN + DB_INIT_LOG + DB_INIT_MPOOL + DB_RECOVER, 0); assert(r == 0);

    double tstart = now();
    assert(nthreads > 0);
    struct arg myargs[nthreads-1];
    toku_pthread_t mytids[nthreads];
    for (uint32_t i = 0; i < nthreads-1; i++) {
        myargs[i] = (struct arg) { env, ntxns, usleep_time};
        r = toku_pthread_create(&mytids[i], NULL, test_thread, &myargs[i]); assert(r == 0);
    }
    test_txn(env, ntxns, usleep_time);
    for (uint32_t i = 0; i < nthreads-1; i++) {
        void *ret;
        r = toku_pthread_join(mytids[i], &ret); assert(r == 0);
    }
    double tend = now();
    double t = tend-tstart;
    double n = ntxns * nthreads;
    printf("%f %f %f\n", n, t, (n/t)*1000000.0);
    r = env->close(env, 0); assert(r == 0);
    return 0;
}
