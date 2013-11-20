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

typedef struct workqueue *WORKQUEUE;
struct workqueue {
    WORKITEM head, tail;        // head and tail of the linked list of work items
    pthread_cond_t wait_read;   // wait for read
    int want_read;              // number of threads waiting to read
    pthread_cond_t wait_write;  // wait for write
    int want_write;             // number of threads waiting to write
    int ninq;                   // number of work items in the queue
    char closed;                // kicks waiting threads off of the write queue
};

// initialize a workqueue
// expects: the workqueue is not initialized
// effects: the workqueue is set to empty and the condition variable is initialized

static void workqueue_init(WORKQUEUE wq) {
    wq->head = wq->tail = 0;
    int r;
    r = pthread_cond_init(&wq->wait_read, 0); assert(r == 0);
    wq->want_read = 0;
    r = pthread_cond_init(&wq->wait_write, 0); assert(r == 0);
    wq->want_write = 0;
    wq->ninq = 0;
    wq->closed = 0;
}

// destroy a workqueue
// expects: the workqueue must be initialized and empty

static void workqueue_destroy(WORKQUEUE wq) {
    assert(wq->head == 0 && wq->tail == 0);
    int r;
    r = pthread_cond_destroy(&wq->wait_read); assert(r == 0);
    r = pthread_cond_destroy(&wq->wait_write); assert(r == 0);
}

// close the workqueue
// effects: signal any threads blocked in the workqueue

static void workqueue_set_closed(WORKQUEUE wq) {
    wq->closed = 1;
    int r;
    r = pthread_cond_broadcast(&wq->wait_read); assert(r == 0);
    r = pthread_cond_broadcast(&wq->wait_write); assert(r == 0);
}

// determine whether or not the write queue is empty
// return: 1 if the write queue is empty, otherwise 0

static int workqueue_empty(WORKQUEUE wq) {
    return wq->head == 0;
}

// put a work item at the tail of the write queue
// expects: the mutex is locked
// effects: append the workitem to the end of the write queue and signal
// any readers

static void workqueue_enq(WORKQUEUE wq, WORKITEM workitem) {
    workitem->next_wq = 0;
    if (wq->tail)
        wq->tail->next_wq = workitem;
    else
        wq->head = workitem;
    wq->tail = workitem;
    wq->ninq++;
    if (wq->want_read) {
        int r = pthread_cond_signal(&wq->wait_read); assert(r == 0);
    }
}

// get a workitem from the head of the write queue
// expects: the mutex is locked
// effects: wait until the workqueue is not empty, remove the first workitem from the
// write queue and return it
// returns: 0 if success, otherwise an error 

static int workqueue_deq(WORKQUEUE wq, pthread_mutex_t *mutex, WORKITEM *workitemptr) {
    while (workqueue_empty(wq)) {
        if (wq->closed)
            return EINVAL;
        wq->want_read++;
        int r = pthread_cond_wait(&wq->wait_read, mutex); assert(r == 0);
        wq->want_read--;
    }
    WORKITEM workitem = wq->head;
    wq->head = workitem->next_wq;
    if (wq->head == 0)
        wq->tail = 0;
    wq->ninq--;
    workitem->next_wq = 0;
    *workitemptr = workitem;
    return 0;
}

#if 0

// suspend the writer thread
// expects: the mutex is locked

static void workqueue_wait_write(WORKQUEUE wq, pthread_mutex_t *mutex) {
    wq->want_write++;
    int r = pthread_cond_wait(&wq->wait_write, mutex); assert(r == 0);
    wq->want_write--;
}

// wakeup the writer threads
// expects: the mutex is locked

static void workqueue_wakeup_write(WORKQUEUE wq) {
    if (wq->want_write) {
        int r = pthread_cond_broadcast(&wq->wait_write); assert(r == 0);
    }
}
        
#endif
