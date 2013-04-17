/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include <util/threadpool.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int verbose = 0;

static int usage(int poolsize) {
    fprintf(stderr, "[-q] [-v] [--verbose] (%d)\n", verbose);
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
    int poolsize = 1;
    int nloops = 100000;

    // options
    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (arg[0] != '-')
            break;
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

        return usage(poolsize);
    }
    int starti = i;

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
