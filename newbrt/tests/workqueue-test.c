#include <toku_portability.h>
#include "test.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>


#include "toku_pthread.h"
#include "memory.h"
#include "workqueue.h"
#include "threadpool.h"

int verbose;

static WORKITEM
new_workitem (void) {
    WORKITEM wi = (WORKITEM) toku_malloc(sizeof *wi); assert(wi);
    return wi;
}

static void
destroy_workitem(WORKITEM wi) {
    toku_free(wi);
}

// test simple create and destroy

static void
test_create_destroy (void) {
    if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    struct workqueue workqueue, *wq = &workqueue;
    workqueue_init(wq);
    assert(workqueue_empty(wq));
    workqueue_destroy(wq);
} 

// verify that the wq implements FIFO ordering

static void
test_simple_enq_deq (int n) {
    if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    struct workqueue workqueue, *wq = &workqueue;
    int r;

    workqueue_init(wq);
    assert(workqueue_empty(wq));
    WORKITEM work[n];
    int i;
    for (i=0; i<n; i++) {
        work[i] = new_workitem();
        workqueue_enq(wq, work[i], 1);
        assert(!workqueue_empty(wq));
    }
    for (i=0; i<n; i++) {
        WORKITEM wi = 0;
        r = workqueue_deq(wq, &wi, 1); 
        assert(r == 0 && wi == work[i]);
        destroy_workitem(wi);
    }
    assert(workqueue_empty(wq));
    workqueue_destroy(wq);
}

// setting the wq closed should cause deq to return EINVAL

static void
test_set_closed (void) {
    if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    struct workqueue workqueue, *wq = &workqueue;
    workqueue_init(wq);
    WORKITEM wi = 0;
    workqueue_set_closed(wq, 1);
    int r = workqueue_deq(wq, &wi, 1);
    assert(r == EINVAL && wi == 0);
    workqueue_destroy(wq);
}

// closing a wq with a blocked reader thread should cause the reader to get EINVAL

static void *
test_set_closed_waiter(void *arg) {
    struct workqueue *wq = arg;
    int r;

    WORKITEM wi = 0;
    r = workqueue_deq(wq, &wi, 1);
    assert(r == EINVAL && wi == 0);
    return arg;
}

static void
test_set_closed_thread (void) {
    if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    struct workqueue workqueue, *wq = &workqueue;
    int r;

    workqueue_init(wq);
    toku_pthread_t tid;
    r = toku_pthread_create(&tid, 0, test_set_closed_waiter, wq); assert(r == 0);
    sleep(1);
    workqueue_set_closed(wq, 1);
    void *ret;
    r = toku_pthread_join(tid, &ret);
    assert(r == 0 && ret == wq);
    workqueue_destroy(wq);
}

// verify writer reader flow control
// the write (main) thread writes as fast as possible until the wq is full. then it
// waits.
// the read thread reads from the wq slowly using a random delay.  it wakes up any
// writers when the wq size <= 1/2 of the wq limit

struct rwfc {
    struct workqueue workqueue;
    int current, limit;
};

static void rwfc_init (struct rwfc *rwfc, int limit) {
    workqueue_init(&rwfc->workqueue);
    rwfc->current = 0; rwfc->limit = limit;
}

static void
rwfc_destroy (struct rwfc *rwfc) {
    workqueue_destroy(&rwfc->workqueue);
}

static void
rwfc_do_read (WORKITEM wi) {
    struct rwfc *rwfc = (struct rwfc *) workitem_arg(wi);
    workqueue_lock(&rwfc->workqueue);
    if (2*rwfc->current-- > rwfc->limit && 2*rwfc->current <= rwfc->limit) {
        workqueue_wakeup_write(&rwfc->workqueue, 0);
    }
    workqueue_unlock(&rwfc->workqueue);
    destroy_workitem(wi);
}

static void *
rwfc_worker (void *arg) {
    struct workqueue *wq = arg;
    while (1) {
        WORKITEM wi = 0;
        int r = workqueue_deq(wq, &wi, 1);
        if (r == EINVAL) {
            assert(wi == 0);
            break;
        }
        usleep(random() % 100);
        wi->f(wi);
    }
    return arg;
}       

static void
test_flow_control (int limit, int n, int maxthreads) {
    if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);

    struct rwfc my_rwfc, *rwfc = &my_rwfc;
    THREADPOOL tp;
    int i;
    rwfc_init(rwfc, limit);
    threadpool_create(&tp, maxthreads);
    for (i=0; i<maxthreads; i++)
        threadpool_maybe_add(tp, rwfc_worker, &rwfc->workqueue);
    sleep(1);     // this is here to block the reader on the first deq
    for (i=0; i<n; i++) {
        WORKITEM wi = new_workitem();
        workitem_init(wi, rwfc_do_read, rwfc);
        workqueue_lock(&rwfc->workqueue);
        workqueue_enq(&rwfc->workqueue, wi, 0);
        rwfc->current++;
        while (rwfc->current >= rwfc->limit) {
            // printf("%d - %d %d\n", i, rwfc->current, rwfc->limit);
            workqueue_wait_write(&rwfc->workqueue, 0);
        }
        workqueue_unlock(&rwfc->workqueue);
        // toku_os_usleep(random() % 1);
    }
    workqueue_set_closed(&rwfc->workqueue, 1);
    threadpool_destroy(&tp);
    rwfc_destroy(rwfc);
}

int
test_main (int argc, const char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0)
            verbose++;
    }
    test_create_destroy();
    test_simple_enq_deq(0);
    test_simple_enq_deq(42);
    test_set_closed();
    test_set_closed_thread();
    test_flow_control(8, 10000, 1);
    test_flow_control(8, 10000, 2);
    test_flow_control(8, 10000, 17);
    return 0;
}
