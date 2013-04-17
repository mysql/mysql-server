/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "toku_portability.h"
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


static u_int32_t numlocks = 0;		        // how many locks ever created?
static u_int32_t pool_high_water_mark = 0;	// max number of locks ever in pool
static u_int32_t num_in_pool = 0;		// number of locks currently in pool

struct leaflock {
    toku_pthread_mutex_t  lock;
    LEAFLOCK              next;
    int id;
};

static LEAFLOCK free_list;
static toku_pthread_mutex_t pool_mutex;

static void
leaflock_pool_lock(void) {
    int r = toku_pthread_mutex_lock(&pool_mutex); assert(r==0);
}

static void
leaflock_pool_unlock(void) {
    int r = toku_pthread_mutex_unlock(&pool_mutex); assert(r==0);
}

void
toku_leaflock_init(void) {
    int r = toku_pthread_mutex_init(&pool_mutex, NULL);
    assert(r == 0);
    free_list = NULL;
}

void
toku_leaflock_destroy(void) {
    leaflock_pool_lock();
    int r;
    assert(num_in_pool==numlocks);
    while (free_list) {
        LEAFLOCK to_free = free_list;
        free_list = free_list->next;
        r = toku_pthread_mutex_destroy(&to_free->lock); assert(r==0);
        toku_free(to_free);
    }
    leaflock_pool_unlock();
    r = toku_pthread_mutex_destroy(&pool_mutex); assert(r == 0);
}

int
toku_leaflock_borrow(LEAFLOCK *leaflockp) {
    leaflock_pool_lock();
    LEAFLOCK loaner;
    int r;
    if (free_list) {
	assert(num_in_pool>0);
	num_in_pool--;
        loaner = free_list;
        free_list = free_list->next;
        r = 0;
    }
    else {
	numlocks++;
        //Create one
        MALLOC(loaner);
        if (loaner==NULL) r = ENOMEM;
        else {
            memset(loaner, 0, sizeof(*loaner));
            loaner->id = numlocks;
            r = toku_pthread_mutex_init(&loaner->lock, NULL); assert(r==0);
        }
    }
    if (r==0) {
        loaner->next = NULL;
        *leaflockp = loaner;
    }
    leaflock_pool_unlock();
    return r;
}

//Caller of this function must be holding the lock being returned.
void
toku_leaflock_unlock_and_return(LEAFLOCK *leaflockp) {
    leaflock_pool_lock();
    LEAFLOCK loaner = *leaflockp;
    *leaflockp = NULL; //Take away caller's reference for good hygiene.
    toku_leaflock_unlock_by_leaf(loaner);
    num_in_pool++;
    if (num_in_pool > pool_high_water_mark)
	pool_high_water_mark = num_in_pool;
    assert (num_in_pool <= numlocks);
    loaner->next = free_list;
    free_list = loaner;
    leaflock_pool_unlock();
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

