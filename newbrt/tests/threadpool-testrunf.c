#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "threadpool.h"

int verbose = 0;

static int usage(int ncpus, int poolsize) {
    fprintf(stderr, "[-q] [-v] [--verbose] (%d)\n", verbose);
    fprintf(stderr, "[--ncpus %d]\n", ncpus);
    fprintf(stderr, "[--poolsize %d]\n", poolsize);
    return 1;
}

static void *f(void *arg) {
    return arg;
}

static void dotest(int poolsize, int nloops) {
    int r;
    struct toku_thread_pool *pool = NULL;
    r = toku_thread_pool_create(&pool, poolsize);
    assert(r == 0 && pool != NULL);

    int i;
    for (i = 0; i < nloops; i++) {
        int n = 1;
        r = toku_thread_pool_run(pool, 1, &n, f, NULL);
        assert(r == 0);
    }

    if (verbose)
        toku_thread_pool_print(pool, stderr);
    toku_thread_pool_destroy(&pool);
}

int main(int argc, char *argv[]) {
    // defaults
    int ncpus = 0;
    int poolsize = 1;
    int nloops = 100000;

    // options
    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (arg[0] != '-')
            break;
        if (strcmp(arg, "--ncpus") == 0 && i+1 < argc) {
            ncpus = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--poolsize") == 0 && i+1 < argc) {
            poolsize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            verbose = verbose+1;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = verbose > 0 ? verbose-1 : 0;
            continue;
        }
    
        return usage(ncpus, poolsize);
    }
    int starti = i;

    if (ncpus > 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (i = 0; i < ncpus; i++)
            CPU_SET(i, &cpuset);
        int r;
        r = sched_setaffinity(getpid(), sizeof cpuset, &cpuset);
        assert(r == 0);
        
        cpu_set_t use_cpuset;
        CPU_ZERO(&use_cpuset);
        r = sched_getaffinity(getpid(), sizeof use_cpuset, &use_cpuset);
        assert(r == 0);
        assert(memcmp(&cpuset, &use_cpuset, sizeof cpuset) == 0);
    }

    if (starti == argc) {
        dotest(poolsize, nloops);
    } else {
        for (i = starti; i < argc; i++) {
            nloops = atoi(argv[i]);
            dotest(poolsize, nloops);
        }
    }
    return 0;
}
