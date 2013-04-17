#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>

#include <toku_portability.h>
#include "toku_os.h"
#include "toku_pthread.h"
#include "threadpool.h"

int verbose;

struct my_threadpool {
    THREADPOOL threadpool;
    toku_pthread_mutex_t mutex;
    toku_pthread_cond_t wait;
    int closed;
    int counter;
};

static void
my_threadpool_init (struct my_threadpool *my_threadpool, int max_threads) {
    int r;
    r = threadpool_create(&my_threadpool->threadpool, max_threads); assert(r == 0);
    assert(my_threadpool != 0);
    r = toku_pthread_mutex_init(&my_threadpool->mutex, 0); assert(r == 0);
    r = toku_pthread_cond_init(&my_threadpool->wait, 0); assert(r == 0);
    my_threadpool->closed = 0;
    my_threadpool->counter = 0;
}

static void
my_threadpool_destroy (struct my_threadpool *my_threadpool, int max_threads) {
    int r;
    r = toku_pthread_mutex_lock(&my_threadpool->mutex); assert(r == 0);
    my_threadpool->closed = 1;
    r = toku_pthread_cond_broadcast(&my_threadpool->wait); assert(r == 0);
    r = toku_pthread_mutex_unlock(&my_threadpool->mutex); assert(r == 0);

    if (verbose) printf("current %d\n", threadpool_get_current_threads(my_threadpool->threadpool));
    threadpool_destroy(&my_threadpool->threadpool); assert(my_threadpool->threadpool == 0);
    assert(my_threadpool->counter == max_threads);
    r = toku_pthread_mutex_destroy(&my_threadpool->mutex); assert(r == 0);
    r = toku_pthread_cond_destroy(&my_threadpool->wait); assert(r == 0);
}

static void *
my_thread_f (void *arg) {
    struct my_threadpool *my_threadpool = arg;
    int r;
    
    r = toku_pthread_mutex_lock(&my_threadpool->mutex); assert(r == 0);
    my_threadpool->counter++;
    while (!my_threadpool->closed) {
        r = toku_pthread_cond_wait(&my_threadpool->wait, &my_threadpool->mutex); assert(r == 0);
    }
    r = toku_pthread_mutex_unlock(&my_threadpool->mutex); assert(r == 0);
    if (verbose) printf("%lu:%s:exit\n", (unsigned long)toku_os_gettid(), __FUNCTION__); 
    return arg;
}

#if defined(__linux__)
#define DO_MALLOC_HOOK 1
#else
#define DO_MALLOC_HOOK 0
#endif
#if DO_MALLOC_HOOK
static void *my_malloc_always_fails(size_t n, const __malloc_ptr_t p) {
    n = n; p = p;
    return 0;
}
#endif

static int
usage (void) {
    printf("threadpool-test: [-v] [-malloc-fail] [N]\n");
    printf("-malloc-fail     simulate malloc failures\n");
    printf("N                max number of threads in the thread pool\n");
    return 1;
}

int
main(int argc, const char *argv[]) {
    int max_threads = 1;
#if defined(__linux__)
    int do_malloc_fail = 0;
#endif

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
#if defined(__linux__)
        } else if (strcmp(arg, "-malloc-fail") == 0) {
            do_malloc_fail = 1;
            continue;
#endif
        } else
            max_threads = atoi(arg);
    }

    struct my_threadpool my_threadpool;
    THREADPOOL threadpool;

    my_threadpool_init(&my_threadpool, max_threads);
    threadpool = my_threadpool.threadpool;
    if (verbose) printf("test threadpool_set_busy\n");
    for (i=0; i<2*max_threads; i++) {
        assert(threadpool_get_current_threads(threadpool) == (i >= max_threads ? max_threads : i));
        threadpool_maybe_add(threadpool, my_thread_f, &my_threadpool);
    }
    assert(threadpool_get_current_threads(threadpool) == max_threads);
    my_threadpool_destroy(&my_threadpool, max_threads);
    
#if DO_MALLOC_HOOK
    if (do_malloc_fail) {
        if (verbose) printf("test threadpool_create with malloc failure\n");
        // test threadpool malloc fails causes ENOMEM
        // glibc supports this.  see malloc.h
        threadpool = 0;

        void *(*orig_malloc_hook) (size_t, const __malloc_ptr_t) = __malloc_hook;
        __malloc_hook = my_malloc_always_fails;
        int r;
        r = threadpool_create(&threadpool, 0); assert(r == ENOMEM);
        r = threadpool_create(&threadpool, 1); assert(r == ENOMEM);
        __malloc_hook = orig_malloc_hook;
    }
#endif

    return 0;
}
