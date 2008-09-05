#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <malloc.h>
#include <pthread.h>
#include <errno.h>

#include "threadpool.h"

// use gcc builtin fetch_and_add 0->no 1->yes
#define DO_ATOMIC_FETCH_AND_ADD 0

struct threadpool {
    int max_threads;
    int current_threads;
    int busy_threads;
    pthread_t pids[];
};

int threadpool_create(THREADPOOL *threadpoolptr, int max_threads) {
    size_t size = sizeof (struct threadpool) + max_threads*sizeof (pthread_t);
    struct threadpool *threadpool = malloc(size);
    if (threadpool == 0)
        return ENOMEM;
    threadpool->max_threads = max_threads;
    threadpool->current_threads = 0;
    threadpool->busy_threads = 0;
    int i;
    for (i=0; i<max_threads; i++) 
        threadpool->pids[i] = 0;
    *threadpoolptr = threadpool;
    return 0;
}

void threadpool_destroy(THREADPOOL *threadpoolptr) {
    struct threadpool *threadpool = *threadpoolptr;
    int i;
    for (i=0; i<threadpool->current_threads; i++) {
        int r; void *ret;
        r = pthread_join(threadpool->pids[i], &ret);
        assert(r == 0);
    }
    *threadpoolptr = 0;
    free(threadpool);
}

void threadpool_maybe_add(THREADPOOL threadpool, void *(*f)(void *), void *arg) {
    if ((threadpool->current_threads == 0 || threadpool->busy_threads < threadpool->current_threads) && threadpool->current_threads < threadpool->max_threads) {
        int r = pthread_create(&threadpool->pids[threadpool->current_threads], 0, f, arg);
        if (r == 0) {
            threadpool->current_threads++;
            threadpool_set_thread_busy(threadpool);
        }
    }
}

void threadpool_set_thread_busy(THREADPOOL threadpool) {
#if DO_ATOMIC_FETCH_AND_ADD
    (void) __sync_fetch_and_add(&threadpool->busy_threads, 1);
#else
    threadpool->busy_threads++;
#endif
}

void threadpool_set_thread_idle(THREADPOOL threadpool) {
#if DO_ATOMIC_FETCH_AND_ADD
    (void) __sync_fetch_and_add(&threadpool->busy_threads, -1);
#else
    threadpool->busy_threads--;
#endif
}

int threadpool_get_current_threads(THREADPOOL threadpool) {
    return threadpool->current_threads;
}
