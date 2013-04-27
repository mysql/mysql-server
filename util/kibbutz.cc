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

#ident "Copyright (c) 2011-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "kibbutz.h"
#include "toku_config.h"
#include <memory.h>
#include <toku_pthread.h>

// A Kibbutz is a collection of workers and some work to do.
struct todo {
    void (*f)(void *extra);
    void *extra;
    struct todo *next;
    struct todo *prev;
};

struct kid {
    struct kibbutz *k;
};

struct kibbutz {
    toku_mutex_t mutex;
    toku_cond_t  cond;
    bool please_shutdown;
    struct todo *head, *tail; // head is the next thing to do.
    int n_workers;
    pthread_t *workers; // an array of n_workers
    struct kid *ids;    // pass this in when creating a worker so it knows who it is.
};

static void *work_on_kibbutz (void *);

KIBBUTZ toku_kibbutz_create (int n_workers) {
    KIBBUTZ XCALLOC(k);
    toku_mutex_init(&k->mutex, NULL);
    toku_cond_init(&k->cond, NULL);
    k->please_shutdown = false;
    k->head = NULL;
    k->tail = NULL;
    k->n_workers = n_workers;
    XMALLOC_N(n_workers, k->workers);
    XMALLOC_N(n_workers, k->ids);
    for (int i=0; i<n_workers; i++) {
        k->ids[i].k = k;
        int r = toku_pthread_create(&k->workers[i], NULL, work_on_kibbutz, &k->ids[i]);
        assert(r==0);
    }
    return k;
}

static void klock (KIBBUTZ k) {
    toku_mutex_lock(&k->mutex);
}
static void kunlock (KIBBUTZ k) {
    toku_mutex_unlock(&k->mutex);
}
static void kwait (KIBBUTZ k) {
    toku_cond_wait(&k->cond, &k->mutex);
}
static void ksignal (KIBBUTZ k) {
    toku_cond_signal(&k->cond);
}

//
// pops the tail of the kibbutz off the list and works on it
// Note that in toku_kibbutz_enq, items are enqueued at the head,
// making the work be done in FIFO order. This is necessary
// to avoid deadlocks in flusher threads.
//
static void *work_on_kibbutz (void *kidv) {
    struct kid *CAST_FROM_VOIDP(kid, kidv);
    KIBBUTZ k = kid->k;
    klock(k);
    while (1) {
        while (k->tail) {
            struct todo *item = k->tail;
            k->tail = item->prev;
            if (k->tail==NULL) {
                k->head=NULL;
            } else {
                // if there are other things to do, then wake up the next guy, if there is one.
                ksignal(k);
            }
            kunlock(k);
            item->f(item->extra);
            toku_free(item);
            klock(k);
            // if there's another item on k->head, then we'll just go grab it now, without waiting for a signal.
        }
        if (k->please_shutdown) {
            // Don't follow this unless the work is all done, so that when we set please_shutdown, all the work finishes before any threads quit.
            ksignal(k); // must wake up anyone else who is waiting, so they can shut down.
            kunlock(k);
            return NULL;
        }
        // There is no work to do and it's not time to shutdown, so wait.
        kwait(k);
    }
}

//
// adds work to the head of the kibbutz 
// Note that in work_on_kibbutz, items are popped off the tail for work,
// making the work be done in FIFO order. This is necessary
// to avoid deadlocks in flusher threads.
//
void toku_kibbutz_enq (KIBBUTZ k, void (*f)(void*), void *extra) {
    struct todo *XMALLOC(td);
    td->f = f;
    td->extra = extra;
    klock(k);
    assert(!k->please_shutdown);
    td->next = k->head;
    td->prev = NULL;
    if (k->head) {
        assert(k->head->prev == NULL);
        k->head->prev = td;
    }
    k->head = td;
    if (k->tail==NULL) k->tail = td;
    ksignal(k);
    kunlock(k);
}

void toku_kibbutz_destroy (KIBBUTZ k)
// Effect: wait for all the enqueued work to finish, and then destroy the kibbutz.
//  Note: It is an error for to perform kibbutz_enq operations after this is called.
{
    klock(k);
    assert(!k->please_shutdown);
    k->please_shutdown = true;
    ksignal(k); // must wake everyone up to tell them to shutdown.
    kunlock(k);
    for (int i=0; i<k->n_workers; i++) {
        void *result;
        int r = toku_pthread_join(k->workers[i], &result);
        assert(r==0);
        assert(result==NULL);
    }
    toku_free(k->workers);
    toku_free(k->ids);
    toku_cond_destroy(&k->cond);
    toku_mutex_destroy(&k->mutex);
    toku_free(k);
}
