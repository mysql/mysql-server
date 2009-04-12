/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <stdio.h>
#include <errno.h>

#include "toku_portability.h"
#include "toku_assert.h"
#include "toku_os.h"
#include "toku_pthread.h"
#include "workqueue.h"
#include "threadpool.h"
#include "toku_worker.h"

// Create fixed number of worker threads, all waiting on a single queue
// of work items (WORKQUEUE).

void toku_init_workers(WORKQUEUE wq, THREADPOOL *tpptr) {
    workqueue_init(wq);
    int nprocs = toku_os_get_number_active_processors();
    int nthreads = nprocs*2;
    threadpool_create(tpptr, nthreads);
    int i;
    for (i=0; i<nthreads; i++)
        threadpool_maybe_add(*tpptr, toku_worker, wq);
}

void toku_destroy_workers(WORKQUEUE wq, THREADPOOL *tpptr) {
    workqueue_set_closed(wq, 1);       // close the work queue and [see "A" in toku_worker()]
    threadpool_destroy(tpptr);         // wait for all of the worker threads to exit
    workqueue_destroy(wq);    
}

void *toku_worker(void *arg) {
    // printf("%lu:%s:start %p\n", toku_pthread_self(), __FUNCTION__, arg);
    WORKQUEUE wq = arg;
    int r;
    while (1) {
        WORKITEM wi = 0;
        r = workqueue_deq(wq, &wi, 1); // get work from the queue, block if empty
        if (r != 0)                    // shut down worker threads when work queue is closed 
            break;                     // [see "A" in toku_destroy_workers() ]
        wi->f(wi);                     // call the work handler function
    }
    // printf("%lu:%s:exit %p\n", toku_pthread_self(), __FUNCTION__, arg);
    return arg;
}
