#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <toku_assert.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

int usage() {
    printf("measure multi-thread work scheduling overhead\n");
    printf("-nthreads N (number of worker threads, default 1)\n");
    printf("-nworkitems N (number of work items, default 1)\n");
    printf("-usleeptime N (work time, default 100)\n");
    printf("-ntests N (number of test iterations, default 1)\n");
    printf("-adaptive (use adaptive mutex locks, default no)\n");
    return 1;
}

typedef struct workitem *WORKITEM;
struct workitem {
    struct workitem *next_wq;
    int usleeptime;
};

#include "workqueue.h"
#include "threadpool.h"

int usleeptime = 100;

void do_work(WORKITEM wi __attribute__((unused))) {
#if 0
    // sleep for usleeptime microseconds
    usleep(usleeptime);
#else
    // busy wait for usleeptime loop interations
    int n = wi->usleeptime;
    volatile int i;
    for (i=0; i<n; i++);
#endif
}

// per thread argument that includes the work queues and locks
struct runner_arg {
    pthread_mutex_t *lock;
    WORKQUEUE wq;
    WORKQUEUE cq;
};

void *runner_thread(void *arg) {
    int r;
    struct runner_arg *runner = (struct runner_arg *)arg;
    r = pthread_mutex_lock(runner->lock); assert(r == 0);
    while (1) {
        WORKITEM wi;
        r = workqueue_deq(runner->wq, runner->lock, &wi);
        if (r != 0) break;
        r = pthread_mutex_unlock(runner->lock); assert(r == 0);
        do_work(wi);
        r = pthread_mutex_lock(runner->lock); assert(r == 0);
        workqueue_enq(runner->cq, wi);
    }    
    r = pthread_mutex_unlock(runner->lock); assert(r == 0);   
    return arg;
}

static inline void lockit(pthread_mutex_t *lock, int nthreads) {
    if (nthreads > 0) {
        int r = pthread_mutex_lock(lock); assert(r == 0);
    }
}

static inline void unlockit(pthread_mutex_t *lock, int nthreads) {
    if (nthreads > 0) {
        int r = pthread_mutex_unlock(lock); assert(r == 0);
    }
}

int main(int argc, char *argv[]) {
    int ntests = 1;
    int nworkitems = 1;
    int nthreads = 1;
    int adaptive = 0;

    int r;
    int i;
    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-help") == 0) {
            return usage();
        }
        if (strcmp(arg, "-ntests") == 0) {
            assert(i+1 < argc);
            ntests = atoi(argv[++i]);
        }
        if (strcmp(arg, "-nworkitems") == 0) {
            assert(i+1 < argc);
            nworkitems = atoi(argv[++i]);
        }
        if (strcmp(arg, "-nthreads") == 0) {
            assert(i+1 < argc);
            nthreads = atoi(argv[++i]);
        }
        if (strcmp(arg, "-usleeptime") == 0) {
            assert(i+1 < argc);
            usleeptime = atoi(argv[++i]);
        }
	if (strcmp(arg, "-adaptive") == 0) {
	  adaptive++;
	}
    }

    pthread_mutex_t lock;
    pthread_mutexattr_t mattr;
    r = pthread_mutexattr_init(&mattr); assert(r == 0);
    if (adaptive) {
        r = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ADAPTIVE_NP); assert(r == 0);
    }
    r = pthread_mutex_init(&lock, &mattr); assert(r == 0);

    struct workqueue wq;
    workqueue_init(&wq);
    struct workqueue cq;
    workqueue_init(&cq);
    THREADPOOL tp;
    r = threadpool_create(&tp, nthreads); assert(r == 0);
    struct runner_arg runner_arg;
    runner_arg.lock = &lock;
    runner_arg.wq = &wq;
    runner_arg.cq = &cq;
    for (i=0; i<nthreads; i++)
        threadpool_maybe_add(tp, runner_thread, &runner_arg);
    int t;
    for (t=0; t<ntests; t++) {
        struct workitem work[nworkitems];
        if (nworkitems == 1) {
            // single work items are run in the main thread
            work[0].usleeptime = usleeptime;
            do_work(&work[0]);
        } else {
            lockit(&lock, nthreads);
            // put all the work on the work queue
            int i;
            for (i=0; i<nworkitems; i++) {
                work[i].usleeptime = usleeptime;
                workqueue_enq(&wq, &work[i]);
            }
            // run some of the work in the main thread
            int ndone = 0;
            while (!workqueue_empty(&wq)) {
                WORKITEM wi;
                workqueue_deq(&wq, &lock, &wi);
                unlockit(&lock, nthreads);
                do_work(wi);
                lockit(&lock, nthreads);
                ndone++;
            }
            // make sure all of the work has completed
            for (i=ndone; i<nworkitems; i++) {
                WORKITEM wi;
                r = workqueue_deq(&cq, &lock, &wi);
                assert(r == 0);
            }
            unlockit(&lock, nthreads);
        }
    }
    workqueue_set_closed(&wq);
    threadpool_destroy(&tp); 
    workqueue_destroy(&wq);
    workqueue_destroy(&cq);
    return 0;
}
