#include "includes.h"

struct threadpool {
    int max_threads;
    int current_threads;
    toku_pthread_t pids[];
};

int threadpool_create(THREADPOOL *threadpoolptr, int max_threads) {
    size_t size = sizeof (struct threadpool) + max_threads*sizeof (toku_pthread_t);
    struct threadpool *threadpool = toku_malloc(size);
    if (threadpool == 0)
        return ENOMEM;
    threadpool->max_threads = max_threads;
    threadpool->current_threads = 0;
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
        r = toku_pthread_join(threadpool->pids[i], &ret);
        assert(r == 0);
    }
    *threadpoolptr = 0;
    toku_free(threadpool);
}

void threadpool_maybe_add(THREADPOOL threadpool, void *(*f)(void *), void *arg) {
    if (threadpool->current_threads < threadpool->max_threads) {
        int r = toku_pthread_create(&threadpool->pids[threadpool->current_threads], 0, f, arg);
        if (r == 0) {
            threadpool->current_threads++;
        }
    }
}

int threadpool_get_current_threads(THREADPOOL threadpool) {
    return threadpool->current_threads;
}
