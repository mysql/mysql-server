/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
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
