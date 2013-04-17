/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include <toku_portability.h>
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <errno.h>
#if defined(HAVE_MALLOC_H)
# include <malloc.h>
#elif defined(HAVE_SYS_MALLOC_H)
# include <sys/malloc.h>
#endif

#include "toku_os.h"
#include <toku_pthread.h>
#include "threadpool.h"

struct my_threadpool {
    THREADPOOL threadpool;
    toku_mutex_t mutex;
    toku_cond_t wait;
    int closed;
    int counter;
};

static void
my_threadpool_init (struct my_threadpool *my_threadpool, int max_threads) {
    int r;
    r = toku_thread_pool_create(&my_threadpool->threadpool, max_threads); assert(r == 0);
    assert(my_threadpool != 0);
    toku_mutex_init(&my_threadpool->mutex, 0);
    toku_cond_init(&my_threadpool->wait, 0);
    my_threadpool->closed = 0;
    my_threadpool->counter = 0;
}

static void
my_threadpool_destroy (struct my_threadpool *my_threadpool, int max_threads) {
    toku_mutex_lock(&my_threadpool->mutex);
    my_threadpool->closed = 1;
    toku_cond_broadcast(&my_threadpool->wait);
    toku_mutex_unlock(&my_threadpool->mutex);

    if (verbose) printf("current %d\n", toku_thread_pool_get_current_threads(my_threadpool->threadpool));
    toku_thread_pool_destroy(&my_threadpool->threadpool); assert(my_threadpool->threadpool == 0);
    assert(my_threadpool->counter == max_threads);
    toku_mutex_destroy(&my_threadpool->mutex);
    toku_cond_destroy(&my_threadpool->wait);
}

static void *
my_thread_f (void *arg) {
    struct my_threadpool *CAST_FROM_VOIDP(my_threadpool, arg);
    toku_mutex_lock(&my_threadpool->mutex);
    my_threadpool->counter++;
    while (!my_threadpool->closed) {
        toku_cond_wait(&my_threadpool->wait, &my_threadpool->mutex);
    }
    toku_mutex_unlock(&my_threadpool->mutex);
    if (verbose) printf("%lu:%s:exit\n", (unsigned long)toku_os_gettid(), __FUNCTION__); 
    return arg;
}

static void *my_malloc_always_fails(size_t n UU()) {
    errno = ENOMEM;
    return NULL;
}

static int
usage (void) {
    printf("threadpool-test: [-v] [-malloc-fail] [N]\n");
    printf("-malloc-fail     simulate malloc failures\n");
    printf("N                max number of threads in the thread pool\n");
    return 1;
}

int
test_main (int argc, const char *argv[]) {
    int max_threads = 1;
    int do_malloc_fail = 0;

    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "-help") == 0) {
            return usage();
        } else if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        } else if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        } else if (strcmp(arg, "-malloc-fail") == 0) {
            do_malloc_fail = 1;
            continue;
        } else
            max_threads = atoi(arg);
    }

    struct my_threadpool my_threadpool;
    THREADPOOL threadpool;

    ZERO_STRUCT(my_threadpool);
    my_threadpool_init(&my_threadpool, max_threads);
    threadpool = my_threadpool.threadpool;
    if (verbose) printf("test threadpool_set_busy\n");
    for (i=0; i<2*max_threads; i++) {
        assert(toku_thread_pool_get_current_threads(threadpool) == (i >= max_threads ? max_threads : i));
        int n = 1;
        toku_thread_pool_run(threadpool, 0, &n, my_thread_f, &my_threadpool);
    }
    assert(toku_thread_pool_get_current_threads(threadpool) == max_threads);
    my_threadpool_destroy(&my_threadpool, max_threads);
    
    if (do_malloc_fail) {
        if (verbose) printf("test threadpool_create with malloc failure\n");
        // test threadpool malloc fails causes ENOMEM

        toku_set_func_malloc(my_malloc_always_fails);
        int r;
        threadpool = NULL;
        r = toku_thread_pool_create(&threadpool, 0); assert(r == ENOMEM);
        r = toku_thread_pool_create(&threadpool, 1); assert(r == ENOMEM);
        toku_set_func_malloc(NULL);
    }

    return 0;
}
