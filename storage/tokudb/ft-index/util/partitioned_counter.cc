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
#include <toku_race_tools.h>
#include <sys/types.h>
#include <pthread.h>

#include "memory.h"
#include "partitioned_counter.h"
#include "doubly_linked_list.h"
#include "growable_array.h"
#include <portability/toku_atomic.h>

#ifdef __APPLE__
// TODO(leif): The __thread declspec is broken in ways I don't understand
// on Darwin.  Partitioned counters use them and it would be prohibitive
// to tease them apart before a week after 6.5.0, so instead, we're just
// not going to use them in the most brutal way possible.  This is a
// terrible implementation of the API in partitioned_counter.h but it
// should be correct enough to release a non-performant version on OSX for
// development.  Soon, we need to either make portable partitioned
// counters, or we need to do this disabling in a portable way.

struct partitioned_counter {
    uint64_t v;
};

PARTITIONED_COUNTER create_partitioned_counter(void) {
    PARTITIONED_COUNTER XCALLOC(counter);
    return counter;
}

void destroy_partitioned_counter(PARTITIONED_COUNTER counter) {
    toku_free(counter);
}

void increment_partitioned_counter(PARTITIONED_COUNTER counter, uint64_t delta) {
    (void) toku_sync_fetch_and_add(&counter->v, delta);
}

uint64_t read_partitioned_counter(PARTITIONED_COUNTER counter) {
    return counter->v;
}

void partitioned_counters_init(void) {}
void partitioned_counters_destroy(void) {}

#else // __APPLE__

//******************************************************************************
//
// Representation: The representation of a partitioned counter comprises a
//  sum, called sum_of_dead; an index, called the ckey, which indexes into a
//  thread-local array to find a thread-local part of the counter; and a
//  linked list of thread-local parts.
//
//  There is also a linked list, for each thread that has a thread-local part
//  of any counter, of all the thread-local parts of all the counters.
//
//  There is a pthread_key which gives us a hook to clean up thread-local
//  state when a thread terminates.  For each thread-local part of a counter
//  that the thread has, we add in the thread-local sum into the sum_of_dead.
//
//  Finally there is a list of all the thread-local arrays so that when we
//  destroy the partitioned counter before the threads are done, we can find
//  and destroy the thread_local_arrays before destroying the pthread_key.
//
// Abstraction function: The sum is represented by the sum of _sum and the
//  sum's of the thread-local parts of the counter.
//
// Representation invariant: Every thread-local part is in the linked list of
//  the thread-local parts of its counter, as well as in the linked list of
//  the counters of a the thread.
//
//******************************************************************************

//******************************************************************************
// The mutex for the PARTITIONED_COUNTER
// We have a single mutex for all the counters because
//  (a) the mutex is obtained infrequently, and
//  (b) it helps us avoid race conditions when destroying the counters.
// The alternative that I couldn't make work is to have a mutex per counter.
//   But the problem is that the counter can be destroyed before threads
//   terminate, or maybe a thread terminates before the counter is destroyed.
//   If the counter is destroyed first, then the mutex is no longer available.
//******************************************************************************

using namespace toku;

static pthread_mutex_t partitioned_counter_mutex = PTHREAD_MUTEX_INITIALIZER;

static void pc_lock (void)
// Effect: Lock the mutex.
{
    int r = pthread_mutex_lock(&partitioned_counter_mutex);
    assert(r==0);
}

static void pc_unlock (void)
// Effect: Unlock the mutex.
{
    int r = pthread_mutex_unlock(&partitioned_counter_mutex);
    assert(r==0);
}

//******************************************************************************
// Key creation primitives
//******************************************************************************
static void pk_create (pthread_key_t *key, void (*destructor)(void*)) {
    int r = pthread_key_create(key, destructor);
    assert(r==0);
}

static void pk_delete (pthread_key_t key) {
    int r = pthread_key_delete(key);
    assert(r==0);
}

static void pk_setspecific (pthread_key_t key, const void *value) {
    int r = pthread_setspecific(key, value);
    assert(r==0);
}

//******************************************************************************
// The counter itself.
// The thread local part of a counter, comprising the thread-local sum a pointer
//  to the partitioned_counter, a pointer to the thread_local list head, and two
//  linked lists. One of the lists is all the thread-local parts that belong to
//  the same counter, and the other is all the thread-local parts that belogn to
//  the same thread.
//******************************************************************************

struct local_counter;

struct partitioned_counter {
    uint64_t       sum_of_dead;                                     // The sum of all thread-local counts from threads that have terminated.
    uint64_t       pc_key;                                          // A unique integer among all counters that have been created but not yet destroyed.
    DoublyLinkedList<struct local_counter *> ll_counter_head; // A linked list of all the thread-local information for this counter.
};

struct local_counter {
    uint64_t                                   sum;                // The thread-local sum.
    PARTITIONED_COUNTER                        owner_pc;           // The partitioned counter that this is part of.
    GrowableArray<struct local_counter *>     *thread_local_array; // The thread local array for this thread holds this local_counter at offset owner_pc->pc_key.
    LinkedListElement<struct local_counter *>  ll_in_counter;      // Element for the doubly-linked list of thread-local information for this PARTITIONED_COUNTER.
};

// Try to get it it into one cache line by aligning it.
static __thread GrowableArray<struct local_counter *> thread_local_array;
static __thread bool                                  thread_local_array_inited = false;

static DoublyLinkedList<GrowableArray<struct local_counter *> *> all_thread_local_arrays;
static __thread LinkedListElement<GrowableArray<struct local_counter *> *> thread_local_ll_elt;

static void destroy_thread_local_part_of_partitioned_counters (void *ignore_me);
static void destroy_thread_local_part_of_partitioned_counters (void *ignore_me __attribute__((__unused__)))
// Effect: This function is called whenever a thread terminates using the
//  destructor of the thread_destructor_key (defined below).  First grab the
//  lock, then go through all the partitioned counters and removes the part that
//  is local to this thread.  We don't actually need the contents of the
//  thread_destructor_key except to cause this function to run.  The content of
//  the key is a static string, so don't try to free it.
{
    pc_lock();
    for (size_t i=0; i<thread_local_array.get_size(); i++) {
        struct local_counter *lc = thread_local_array.fetch_unchecked(i);
        if (lc==NULL) continue;
        PARTITIONED_COUNTER owner = lc->owner_pc;
        owner->sum_of_dead += lc->sum;
        owner->ll_counter_head.remove(&lc->ll_in_counter);
        toku_free(lc);
    }
    all_thread_local_arrays.remove(&thread_local_ll_elt);
    thread_local_array_inited = false;
    thread_local_array.deinit();
    pc_unlock();
}

//******************************************************************************
// We employ a system-wide pthread_key simply to get a notification when a
//  thread terminates. The key will simply contain a constant string (it's "dont
//  care", but it doesn't matter what it is, as long as it's not NULL.  We need
//  a constructor function to set up the pthread_key.  We used a constructor
//  function intead of a C++ constructor because that's what we are used to,
//  rather than because it's necessarily better.  Whenever a thread tries to
//  increment a partitioned_counter for the first time, it sets the
//  pthread_setspecific for the thread_destructor_key.  It's OK if the key gets
//  setspecific multiple times, it's always the same value.  When a thread (that
//  has created a thread-local part of any partitioned counter) terminates, the
//  destroy_thread_local_part_of_partitioned_counters will run.  It may run
//  before or after other pthread_key destructors, but the thread-local
//  ll_thread_head variable is still present until the thread is completely done
//  running.
//******************************************************************************

static pthread_key_t thread_destructor_key;

//******************************************************************************
// We don't like using up pthread_keys (macos provides only 128 of them),
// so we built our own.   Also, looking at the source code for linux libc,
// it looks like pthread_keys get slower if there are a lot of them.
// So we use only one pthread_key.
//******************************************************************************

GrowableArray<bool> counters_in_use;

static uint64_t allocate_counter (void)
// Effect: Find an unused counter number, and allocate it, returning the counter number.
//  Grabs the pc_lock.
{
    uint64_t ret;
    pc_lock();
    size_t size = counters_in_use.get_size();
    for (uint64_t i=0; i<size; i++) {
        if (!counters_in_use.fetch_unchecked(i)) {
            counters_in_use.store_unchecked(i, true);
            ret = i;
            goto unlock;
        }
    }
    counters_in_use.push(true);
    ret = size;
unlock:
    pc_unlock();
    return ret;
}


static void free_counter(uint64_t counternum)
// Effect: Free a counter.
// Requires: The pc mutex is held before calling.
{
    assert(counternum < counters_in_use.get_size());
    assert(counters_in_use.fetch_unchecked(counternum));
    counters_in_use.store_unchecked(counternum, false);
}

static void destroy_counters (void) {
    counters_in_use.deinit();
}


//******************************************************************************
// Now for the code that actually creates a counter.
//******************************************************************************

PARTITIONED_COUNTER create_partitioned_counter(void)
// Effect: Create a counter, initialized to zero.
{
    PARTITIONED_COUNTER XMALLOC(result);
    result->sum_of_dead = 0;
    result->pc_key = allocate_counter();
    result->ll_counter_head.init();
    return result;
}

void destroy_partitioned_counter(PARTITIONED_COUNTER pc)
// Effect: Destroy the counter.  No operations on this counter are permitted after.
// Implementation note: Since we have a global lock, we can destroy all the thread-local
//  versions as well.
{
    pc_lock();
    uint64_t pc_key = pc->pc_key;
    LinkedListElement<struct local_counter *> *first;
    while (pc->ll_counter_head.pop(&first)) {
        // We just removed first from the counter list, now we must remove it from the thread-local array.
        struct local_counter *lc = first->get_container();
        assert(pc == lc->owner_pc);
        GrowableArray<struct local_counter *> *tla = lc->thread_local_array;
        tla->store_unchecked(pc_key, NULL);
        toku_free(lc);
    }
    toku_free(pc);
    free_counter(pc_key);
    pc_unlock();
}

static inline struct local_counter *get_thread_local_counter(uint64_t pc_key, GrowableArray<struct local_counter *> *a)
{
    if (pc_key >= a->get_size()) {
        return NULL;
    } else {
        return a->fetch_unchecked(pc_key);
    }
}

static struct local_counter *get_or_alloc_thread_local_counter(PARTITIONED_COUNTER pc)
{
    // Only this thread is allowed to modify thread_local_array, except for setting tla->array[pc_key] to NULL
    // when a counter is destroyed (and in that case there should be no race because no other thread should be
    // trying to access the same local counter at the same time.
    uint64_t pc_key = pc->pc_key;
    struct local_counter *lc = get_thread_local_counter(pc->pc_key, &thread_local_array);
    if (lc == NULL) {
        XMALLOC(lc);    // Might as well do the malloc without holding the pc lock.  But most of the rest of this work needs the lock.
        pc_lock();

        // Set things up so that this thread terminates, the thread-local parts of the counter will be destroyed and merged into their respective counters.
        if (!thread_local_array_inited) {
            pk_setspecific(thread_destructor_key, "dont care");
            thread_local_array_inited=true;
            thread_local_array.init();
            all_thread_local_arrays.insert(&thread_local_ll_elt, &thread_local_array);
        }

        lc->sum         = 0;
        TOKU_VALGRIND_HG_DISABLE_CHECKING(&lc->sum, sizeof(lc->sum)); // the counter increment is kind of racy.
        lc->owner_pc    = pc;
        lc->thread_local_array = &thread_local_array;

        // Grow the array if needed, filling in NULLs
        while (thread_local_array.get_size() <= pc_key) {
            thread_local_array.push(NULL);
        }
        thread_local_array.store_unchecked(pc_key, lc);
        pc->ll_counter_head.insert(&lc->ll_in_counter, lc);
        pc_unlock();
    }
    return lc;
}

void increment_partitioned_counter(PARTITIONED_COUNTER pc, uint64_t amount)
// Effect: Increment the counter by amount.
// Requires: No overflows.  This is a 64-bit unsigned counter.
{
    struct local_counter *lc = get_or_alloc_thread_local_counter(pc);
    lc->sum += amount;
}

static int sumit(struct local_counter *lc, uint64_t *sum) {
    (*sum)+=lc->sum;
    return 0;
}

uint64_t read_partitioned_counter(PARTITIONED_COUNTER pc)
// Effect: Return the current value of the counter.
// Implementation note: Sum all the thread-local counts along with the sum_of_the_dead.
{
    pc_lock();
    uint64_t sum = pc->sum_of_dead;
    int r = pc->ll_counter_head.iterate<uint64_t *>(sumit, &sum);
    assert(r==0);
    pc_unlock();
    return sum;
}

void partitioned_counters_init(void)
// Effect: Initialize any partitioned counters data structures that must be set up before any partitioned counters run.
{
    pk_create(&thread_destructor_key, destroy_thread_local_part_of_partitioned_counters);
    all_thread_local_arrays.init();
}

void partitioned_counters_destroy(void)
// Effect: Destroy any partitioned counters data structures.
{
    pc_lock();
    LinkedListElement<GrowableArray<struct local_counter *> *> *a_ll;
    while (all_thread_local_arrays.pop(&a_ll)) {
        a_ll->get_container()->deinit();
    }

    pk_delete(thread_destructor_key);
    destroy_counters();
    pc_unlock();
}

#endif // __APPLE__
