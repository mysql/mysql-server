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
#include <toku_assert.h>
#include <malloc.h>
#include <pthread.h>
#include <errno.h>

#include "threadpool.h"
#include <portability/toku_atomic.h>

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
    struct threadpool *threadpool = (struct threadpool *) malloc(size);
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
    if (threadpool->current_threads < threadpool->max_threads) {
        int r = pthread_create(&threadpool->pids[threadpool->current_threads], 0, f, arg);
        if (r == 0) {
            threadpool->current_threads++;
            threadpool_set_thread_busy(threadpool);
        }
    }
}

void threadpool_set_thread_busy(THREADPOOL threadpool) {
#if DO_ATOMIC_FETCH_AND_ADD
    (void) toku_sync_fetch_and_add(&threadpool->busy_threads, 1);
#else
    threadpool->busy_threads++;
#endif
}

void threadpool_set_thread_idle(THREADPOOL threadpool) {
#if DO_ATOMIC_FETCH_AND_ADD
    (void) toku_sync_fetch_and_add(&threadpool->busy_threads, -1);
#else
    threadpool->busy_threads--;
#endif
}

int threadpool_get_current_threads(THREADPOOL threadpool) {
    return threadpool->current_threads;
}
