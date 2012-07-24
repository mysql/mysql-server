/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "partitioned_counter.h"
#include "memory.h"
#include <pthread.h>
#include <valgrind/helgrind.h>

struct local_counter {
    unsigned long        sum;
    struct local_counter *prev, *next;
    PARTITIONED_COUNTER   owner;
};

struct partitioned_counter {
    unsigned long   sum_of_dead;
    pthread_key_t   key;
    struct local_counter *first, *last;
};

// We have a single mutex for all the counters because
//  (a) the mutex is obtained infrequently, and
//  (b) it helps us avoid race conditions when destroying the counters.
static pthread_mutex_t partitioned_counter_mutex = PTHREAD_MUTEX_INITIALIZER;


static void pc_lock (void) {
    int r = pthread_mutex_lock(&partitioned_counter_mutex);
    assert(r==0);
}
static void pc_unlock (void) {
    int r = pthread_mutex_unlock(&partitioned_counter_mutex);
    assert(r==0);
}

static void local_destroy_counter (void *counterp) {
    pc_lock();
    struct local_counter *CAST_FROM_VOIDP(lc, counterp);
    PARTITIONED_COUNTER owner = lc->owner;
    // Save the sum
    owner->sum_of_dead += lc->sum;
    // Remove from linked list.
    if (lc->prev) {
	lc->prev->next = lc->next;
    } else {
	owner->first = lc->next;
    }
    if (lc->next) {
	lc->next->prev = lc->prev;
    } else {
	owner->last = lc->prev;
    }
    
    // Free the local part of the counter and return.
    toku_free(lc);
    {
	int r = pthread_setspecific(owner->key, NULL);
	assert(r==0);
    }
    pc_unlock();
}


PARTITIONED_COUNTER create_partitioned_counter(void) 
// Effect: Create a counter, initialized to zero.
{
    PARTITIONED_COUNTER MALLOC(result);
    result->sum_of_dead = 0;
    {
	int r = pthread_key_create(&result->key, local_destroy_counter);
	assert(r==0);
    }
    result->first = NULL;
    result->last  = NULL;
    return result;
}

void destroy_partitioned_counter (PARTITIONED_COUNTER pc)
// Effect: Destroy the counter.  No operations on that counter are permitted after this.
// Implementation note: Since we have a global lock, we can destroy all the key-specific versions as well.
{
    pc_lock();
    while (pc->first) {
	struct local_counter *next = pc->first->next;
	assert(pc->first->owner==pc);
	toku_free(pc->first);
	pc->first = next;
    }
    {
	int r = pthread_key_delete(pc->key);
	assert(r==0);
    }
    toku_free(pc);
    pc_unlock();
}

void increment_partitioned_counter (PARTITIONED_COUNTER pc, unsigned long amount)
// Effect: Increment the counter by amount.
// Requires: No overflows.  This is a 64-bit unsigned counter.
// Requires: You may not increment this after a destroy has occured.
{
    struct local_counter *CAST_FROM_VOIDP(lc, pthread_getspecific(pc->key));
    if (lc==NULL) {
	pc_lock();
	MALLOC(lc);
	lc->sum = 0;
	HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&lc->sum, sizeof(lc->sum)); // the counter increment is kind of racy.
	lc->prev = pc->last;
	lc->next = NULL;
	lc->owner = pc;
	if (pc->first==NULL) {
	    pc->first = lc;
	} else {
	    pc->last->next = lc;
	}
	pc->last = lc;
	if (pc->first==NULL) pc->first=lc;
	int r = pthread_setspecific(pc->key, lc);
	assert(r==0);
	pc_unlock();
    }
    lc->sum += amount;
}

unsigned long read_partitioned_counter (PARTITIONED_COUNTER pc)
// Effect: Return the current value of the counter.
{
    pc_lock();
    unsigned long sum = pc->sum_of_dead;
    for (struct local_counter *lc = pc->first; lc; lc=lc->next) {
	sum += lc->sum;
    }
    pc_unlock();
    return sum;
}
