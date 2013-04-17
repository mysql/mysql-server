#ifndef _TOKU_WORKSET_H
#define _TOKU_WORKSET_H

#include <toku_list.h>
#include <toku_pthread.h>

// the work struct is the base class for work to be done by some threads
struct work {
    struct toku_list next;
};

// the workset struct contains the set of work to be done by some threads
// the lock protects the work list
struct workset {
    toku_pthread_mutex_t lock;
    struct toku_list worklist;
};

static inline void workset_init(struct workset *ws) {
    int r = toku_pthread_mutex_init(&ws->lock, NULL); assert(r == 0);
    toku_list_init(&ws->worklist);
}

static inline void workset_destroy(struct workset *ws) {
    assert(toku_list_empty(&ws->worklist));
    int r = toku_pthread_mutex_destroy(&ws->lock); assert(r == 0);
}

static inline void workset_lock(struct workset *ws) {
    int r = toku_pthread_mutex_lock(&ws->lock); assert(r == 0);
}
        
static inline void workset_unlock(struct workset *ws) {
    int r = toku_pthread_mutex_unlock(&ws->lock); assert(r == 0);
}

// put work in the workset 
static inline void workset_put(struct workset *ws, struct work *w) {
    workset_lock(ws);
    toku_list_push(&ws->worklist, &w->next);
    workset_unlock(ws);
}

// put work in the workset.  assume already locked.
static inline void workset_put_locked(struct workset *ws, struct work *w) {
    toku_list_push(&ws->worklist, &w->next);
}

// get work from the workset
static inline struct work *workset_get(struct workset *ws) {
    workset_lock(ws);
    struct work *w = NULL;
    if (!toku_list_empty(&ws->worklist)) {
        struct toku_list *l = toku_list_pop_head(&ws->worklist);
        w = toku_list_struct(l, struct work, next);
    }
    workset_unlock(ws);
    return w;
}

// create a set of threads to run a given function
// tids will contain the thread id's of the created threads
// *ntids on input contains the number of threads requested, on output contains the number of threads created
static inline void threadset_create(toku_pthread_t tids[], int *ntids, void *(*f)(void *arg), void *arg) {
    int n = *ntids;
    int i;
    for (i = 0; i < n; i++) {
        int r = toku_pthread_create(&tids[i], NULL, f, arg); 
        if (r != 0)
            break;
    }
    *ntids = i;
}

// join with a set of threads
static inline void threadset_join(toku_pthread_t tids[], int ntids) {
    for (int i = 0; i < ntids; i++) {
        void *ret;
        int r = toku_pthread_join(tids[i], &ret); assert(r == 0);
    }
}

#endif
