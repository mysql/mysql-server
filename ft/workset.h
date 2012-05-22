/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef _TOKU_WORKSET_H
#define _TOKU_WORKSET_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_list.h>
#include <toku_pthread.h>

// The work struct is the base class for work to be done by some threads
struct work {
    struct toku_list next;
};

// The workset struct contains the set of work to be done by some threads
struct workset {
    toku_mutex_t lock;
    struct toku_list worklist;           // a list of work
    int refs;                            // number of workers that have a reference on the workset
    toku_cond_t worker_wait;     // a condition variable used to wait for all of the worker to release their reference on the workset
};

static inline void 
workset_init(struct workset *ws) {
    toku_mutex_init(&ws->lock, NULL);
    toku_list_init(&ws->worklist);
    ws->refs = 1;      // the calling thread gets a reference
    toku_cond_init(&ws->worker_wait, NULL);
}

static inline void 
workset_destroy(struct workset *ws) {
    invariant(toku_list_empty(&ws->worklist));
    toku_cond_destroy(&ws->worker_wait);
    toku_mutex_destroy(&ws->lock);
}

static inline void 
workset_lock(struct workset *ws) {
    toku_mutex_lock(&ws->lock);
}
        
static inline void 
workset_unlock(struct workset *ws) {
    toku_mutex_unlock(&ws->lock);
}

// Put work in the workset.  Assume the workset is already locked.
static inline void 
workset_put_locked(struct workset *ws, struct work *w) {
    toku_list_push(&ws->worklist, &w->next);
}

// Put work in the workset 
static inline void 
workset_put(struct workset *ws, struct work *w) {
    workset_lock(ws);
    workset_put_locked(ws, w);
    workset_unlock(ws);
}

// Get work from the workset
static inline struct work *
workset_get(struct workset *ws) {
    workset_lock(ws);
    struct work *w = NULL;
    if (!toku_list_empty(&ws->worklist)) {
        struct toku_list *l = toku_list_pop_head(&ws->worklist);
        w = toku_list_struct(l, struct work, next);
    }
    workset_unlock(ws);
    return w;
}

// Add references to the workset
static inline void 
workset_add_ref(struct workset *ws, int refs) {
    workset_lock(ws);
    ws->refs += refs;
    workset_unlock(ws);
}

// Release a reference on the workset
static inline void 
workset_release_ref(struct workset *ws) {
    workset_lock(ws);
    if (--ws->refs == 0) {
        toku_cond_broadcast(&ws->worker_wait);
    }
    workset_unlock(ws);
}

// Wait until all of the worker threads have released their reference on the workset
static inline void 
workset_join(struct workset *ws) {
    workset_lock(ws);
    while (ws->refs != 0) {
        toku_cond_wait(&ws->worker_wait, &ws->lock);
    }
    workset_unlock(ws);
}

#endif
