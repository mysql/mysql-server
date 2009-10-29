/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include "toku_pthread.h"
#include "leaflock.h"
#include "toku_assert.h"
#include "memory.h"
#include <errno.h>
#include <string.h>

// See ticket 1423.
//
// Purpose of this file is to manage a pool of locks that are used
// to lock brt leaf nodes and the cursors that point to them.
// Each lock protects one set of cursors, the cursors that point
// to a single brt leaf node.  (Actually, the cursors point to
// a leaf node's omt.)
// Because the cursors that point to a brt leaf node are organized
// in a linked list (whose head is in the brt leaf node), the
// operations listed below are not threadsafe.
//
// It is necessary to hold the lock around following operations:
// - associate cursor with brt block (brt_cursor_update())
//   [ puts cursor on linked list ]
// - invalidate cursor (done only by search path)
//   [ removes cursor from linked list ]
// - invalidate all cursors associated with a brt block
//   (done only by (a) writer thread or (b) insert/delete,
//   at least until we have brt-node-level locks)
//   [ removes all cursors from linked list ]
//
// When a leaf is created, it borrows ownership of a leaflock.
// The leaf has a reference to the leaflock.
//
// When a leaf is evicted, ownership of the leaflock returns to the
// pool of available leaflocks.
//
// The reason an unused leaflock (which is no longer associated with
// any particular leaf node) is kept in a pool (rather than destroyed)
// is that some client thread may be waiting on the lock or about to
// request the lock.
//
// The brt leaf node has a reference to the leaflock, and there is
// a reference to the leaflock in every cursor that references the
// brt leaf node.
//


struct leaflock {
    toku_pthread_mutex_t  lock;
    LEAFLOCK              next;
    int id;
};

struct leaflock_pool {
    LEAFLOCK free_list;
    toku_pthread_mutex_t pool_mutex;
    u_int32_t numlocks;		        // how many locks ever created?
    u_int32_t pool_high_water_mark;	// max number of locks ever in pool
    u_int32_t num_in_pool;		// number of locks currently in pool
};

static void
leaflock_pool_lock(LEAFLOCK_POOL pool) {
    int r = toku_pthread_mutex_lock(&pool->pool_mutex); assert(r==0);
}

static void
leaflock_pool_unlock(LEAFLOCK_POOL pool) {
    int r = toku_pthread_mutex_unlock(&pool->pool_mutex); assert(r==0);
}

int
toku_leaflock_create(LEAFLOCK_POOL* pool) {
    int r;
    LEAFLOCK_POOL XCALLOC(result);
    if (!result) r = ENOMEM;
    else {
        r = toku_pthread_mutex_init(&result->pool_mutex, NULL);
        assert(r == 0);
        *pool = result;
    }
    return r;
}

int
toku_leaflock_destroy(LEAFLOCK_POOL* pool_p) {
    LEAFLOCK_POOL pool = *pool_p;
    *pool_p = NULL;
    leaflock_pool_lock(pool);
    int r;
    assert(pool->num_in_pool==pool->numlocks);
    while (pool->free_list) {
        LEAFLOCK to_free = pool->free_list;
        pool->free_list = pool->free_list->next;
        r = toku_pthread_mutex_destroy(&to_free->lock); assert(r==0);
        toku_free(to_free);
    }
    leaflock_pool_unlock(pool);
    r = toku_pthread_mutex_destroy(&pool->pool_mutex); assert(r == 0);
    toku_free(pool);
    return r;
}

int
toku_leaflock_borrow(LEAFLOCK_POOL pool, LEAFLOCK *leaflockp) {
    leaflock_pool_lock(pool);
    LEAFLOCK loaner;
    int r;
    if (pool->free_list) {
	assert(pool->num_in_pool>0);
	pool->num_in_pool--;
        loaner = pool->free_list;
        pool->free_list = pool->free_list->next;
        r = 0;
    }
    else {
	pool->numlocks++;
        //Create one
        CALLOC(loaner);
        if (loaner==NULL) r = ENOMEM;
        else {
            loaner->id = pool->numlocks;
            r = toku_pthread_mutex_init(&loaner->lock, NULL); assert(r==0);
        }
    }
    if (r==0) {
        loaner->next = NULL;
        *leaflockp = loaner;
    }
    leaflock_pool_unlock(pool);
    return r;
}

//Caller of this function must be holding the lock being returned.
void
toku_leaflock_unlock_and_return(LEAFLOCK_POOL pool, LEAFLOCK *leaflockp) {
    leaflock_pool_lock(pool);
    LEAFLOCK loaner = *leaflockp;
    *leaflockp = NULL; //Take away caller's reference for good hygiene.
    toku_leaflock_unlock_by_leaf(loaner);
    pool->num_in_pool++;
    if (pool->num_in_pool > pool->pool_high_water_mark)
	pool->pool_high_water_mark = pool->num_in_pool;
    assert (pool->num_in_pool <= pool->numlocks);
    loaner->next = pool->free_list;
    pool->free_list = loaner;
    leaflock_pool_unlock(pool);
}

void
toku_leaflock_lock_by_leaf(LEAFLOCK leaflock) {
    assert(leaflock->next==NULL);
    int r = toku_pthread_mutex_lock(&leaflock->lock); assert(r==0);
}

void
toku_leaflock_unlock_by_leaf(LEAFLOCK leaflock) {
    assert(leaflock->next==NULL);
    int r = toku_pthread_mutex_unlock(&leaflock->lock); assert(r==0);
}

void
toku_leaflock_lock_by_cursor(LEAFLOCK leaflock) {
    int r = toku_pthread_mutex_lock(&leaflock->lock); assert(r==0);
}

void
toku_leaflock_unlock_by_cursor(LEAFLOCK leaflock) {
    int r = toku_pthread_mutex_unlock(&leaflock->lock); assert(r==0);
}

