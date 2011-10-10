/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <stdbool.h>
#include "toku_pthread.h"
#include "kibbutz.h"
#include "includes.h"

// A Kibbutz is a collection of workers and some work to do.
struct todo {
    void (*f)(void *extra);
    void *extra;
    struct todo *next;
    struct todo *prev;
};

struct kid {
    struct kibbutz *k;
    int id;
};

struct kibbutz {
    toku_pthread_mutex_t mutex;
    toku_pthread_cond_t  cond;
    bool please_shutdown;
    struct todo *head, *tail; // head is the next thing to do.
    int n_workers;
    pthread_t *workers; // an array of n_workers
    struct kid *ids;    // pass this in when creating a worker so it knows who it is.
};

static void *work_on_kibbutz (void *);

KIBBUTZ toku_kibbutz_create (int n_workers) {
    KIBBUTZ XMALLOC(k);
    { int r = toku_pthread_mutex_init(&k->mutex, NULL); assert(r==0); }
    { int r = toku_pthread_cond_init(&k->cond, NULL);   assert(r==0); }
    k->please_shutdown = false;
    k->head = NULL;
    k->tail = NULL;
    k->n_workers = n_workers;
    XMALLOC_N(n_workers, k->workers);
    XMALLOC_N(n_workers, k->ids);
    for (int i=0; i<n_workers; i++) {
        k->ids[i].k = k;
        k->ids[i].id = i;
        int r = toku_pthread_create(&k->workers[i], NULL, work_on_kibbutz, &k->ids[i]);
        assert(r==0);
    }
    return k;
}

static void klock (KIBBUTZ k) {
    int r = toku_pthread_mutex_lock(&k->mutex);
    assert(r==0);
}
static void kunlock (KIBBUTZ k) {
    int r = toku_pthread_mutex_unlock(&k->mutex);
    assert(r==0);
}
static void kwait (KIBBUTZ k) {
    int r = toku_pthread_cond_wait(&k->cond, &k->mutex);
    assert(r==0);
}
static void ksignal (KIBBUTZ k) {
    int r = toku_pthread_cond_signal(&k->cond);
    assert(r==0);
}

static void *work_on_kibbutz (void *kidv) {
    struct kid *kid = kidv;
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
    { int r = toku_pthread_cond_destroy(&k->cond);  assert(r==0); }
    { int r = toku_pthread_mutex_destroy(&k->mutex);  assert(r==0); }
    toku_free(k);
}
