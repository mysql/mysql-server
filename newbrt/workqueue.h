/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef _TOKU_WORKQUEUE_H
#define _TOKU_WORKQUEUE_H

#include <errno.h>
#include "toku_assert.h"
#include "toku_pthread.h"

struct workitem;

// A work function is called by a worker thread when the workitem (see below) is being handled
// by a worker thread.
typedef void (*WORKFUNC)(struct workitem *wi);

// A workitem contains the function that is called by a worker thread in a threadpool.
// A workitem is queued in a workqueue.
typedef struct workitem *WORKITEM;
struct workitem {
    WORKFUNC f;
    void *arg;
    struct workitem *next;
};

// Initialize a workitem with a function and argument
static inline void workitem_init(WORKITEM wi, WORKFUNC f, void *arg) {
    wi->f = f;
    wi->arg = arg;
    wi->next = 0;
}

// Access the workitem function
static inline WORKFUNC workitem_func(WORKITEM wi) {
    return wi->f;
}

// Access the workitem argument
static inline void *workitem_arg(WORKITEM wi) {
    return wi->arg;
}

// A workqueue is currently a fifo of workitems that feeds a thread pool.  We may
// divide the workqueue into per worker thread queues.
typedef struct workqueue *WORKQUEUE;
struct workqueue {
    WORKITEM head, tail;             // list of workitem's
    toku_pthread_mutex_t lock;
    toku_pthread_cond_t wait_read;   // wait for read
    int want_read;                   // number of threads waiting to read
    toku_pthread_cond_t wait_write;  // wait for write
    int want_write;                  // number of threads waiting to write
    char closed;                     // kicks waiting threads off of the write queue
};

// Get a pointer to the workqueue lock.  This is used by workqueue client software
// that wants to control the workqueue locking.
static inline toku_pthread_mutex_t *workqueue_lock_ref(WORKQUEUE wq) {
    return &wq->lock;
}

// Lock the workqueue
static inline void workqueue_lock(WORKQUEUE wq) {
    int r = toku_pthread_mutex_lock(&wq->lock); assert(r == 0);
}

// Unlock the workqueue
static inline void workqueue_unlock(WORKQUEUE wq) {
    int r = toku_pthread_mutex_unlock(&wq->lock); assert(r == 0);
}

// Initialize a workqueue
// Expects: the workqueue is not initialized
// Effects: the workqueue is set to empty and the condition variable is initialized
__attribute__((unused))
static void workqueue_init(WORKQUEUE wq) {
    int r;
    r = toku_pthread_mutex_init(&wq->lock, 0); assert(r == 0);
    wq->head = wq->tail = 0;
    r = toku_pthread_cond_init(&wq->wait_read, 0); assert(r == 0);
    wq->want_read = 0;
    r = toku_pthread_cond_init(&wq->wait_write, 0); assert(r == 0);
    wq->want_write = 0;
    wq->closed = 0;
}

// Destroy a work queue
// Expects: the work queue must be initialized and empty
__attribute__((unused))
static void workqueue_destroy(WORKQUEUE wq) {
    int r;
    workqueue_lock(wq); // shutup helgrind
    assert(wq->head == 0 && wq->tail == 0);
    workqueue_unlock(wq);
    r = toku_pthread_cond_destroy(&wq->wait_read); assert(r == 0);
    r = toku_pthread_cond_destroy(&wq->wait_write); assert(r == 0);
    r = toku_pthread_mutex_destroy(&wq->lock); assert(r == 0);
}

// Close the work queue
// Effects: signal any threads blocked in the work queue
__attribute__((unused))
static void workqueue_set_closed(WORKQUEUE wq, int dolock) {
    int r;
    if (dolock) workqueue_lock(wq);
    wq->closed = 1;
    if (dolock) workqueue_unlock(wq);
    r = toku_pthread_cond_broadcast(&wq->wait_read); assert(r == 0);
    r = toku_pthread_cond_broadcast(&wq->wait_write); assert(r == 0);
}

// Determine whether or not the work queue is empty
// Returns: 1 if the work queue is empty, otherwise 0
static inline int workqueue_empty(WORKQUEUE wq) {
    return wq->head == 0;
}

// Put a work item at the tail of the work queue
// Effects: append the work item to the end of the work queue and signal
// any work queue readers.
// Dolock controls whether or not the work queue lock should be taken.
__attribute__((unused))
static void workqueue_enq(WORKQUEUE wq, WORKITEM wi, int dolock) {
    if (dolock) workqueue_lock(wq);
    wi->next = 0;
    if (wq->tail)
        wq->tail->next = wi;
    else
        wq->head = wi;
    wq->tail = wi;
    if (wq->want_read) {
        int r = toku_pthread_cond_signal(&wq->wait_read); assert(r == 0);
    }
    if (dolock) workqueue_unlock(wq);
}

// Get a work item from the head of the work queue
// Effects: wait until the workqueue is not empty, remove the first workitem from the
// queue and return it.
// Dolock controls whether or not the work queue lock should be taken.
// Success: returns 0 and set the wiptr
// Failure: returns non-zero
__attribute__((unused))
static int workqueue_deq(WORKQUEUE wq, WORKITEM *wiptr, int dolock) {
    if (dolock) workqueue_lock(wq);
    while (workqueue_empty(wq)) {
        if (wq->closed) {
            if (dolock) workqueue_unlock(wq);
            return EINVAL;
        }
        wq->want_read++;
        int r = toku_pthread_cond_wait(&wq->wait_read, &wq->lock); assert(r == 0);
        wq->want_read--;
    }
    WORKITEM wi = wq->head;
    wq->head = wi->next;
    if (wq->head == 0)
        wq->tail = 0;
    wi->next = 0;
    if (dolock) workqueue_unlock(wq);
    *wiptr = wi;
    return 0;
}

// Suspend a work queue writer thread
__attribute__((unused))
static void workqueue_wait_write(WORKQUEUE wq, int dolock) {
    if (dolock) workqueue_lock(wq);
    wq->want_write++;
    int r = toku_pthread_cond_wait(&wq->wait_write, &wq->lock); assert(r == 0);
    wq->want_write--;
    if (dolock) workqueue_unlock(wq);
}

// Wakeup the waiting work queue writer threads
__attribute__((unused))
static void workqueue_wakeup_write(WORKQUEUE wq, int dolock) {
    if (wq->want_write) {
        if (dolock) workqueue_lock(wq);
        if (wq->want_write) {
            int r = toku_pthread_cond_broadcast(&wq->wait_write); assert(r == 0);
        }
        if (dolock) workqueue_unlock(wq);
    }
}

#endif
