/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <valgrind/helgrind.h>
#include "partitioned_counter.h"
#include "memory.h"

#include <sys/types.h>

//******************************************************************************
// Representation: The representation of a partitioned counter
//  comprises a sum, called _sum_of_dead, a pthread_key called _key,
//  and a linked list of thread-local parts.
//  There is also a linked list, for each thread that has a
//  thread-local part of any counter, of all the thread-local parts of
//  all the counters.
// Abstraction function: The sum is represented by the sum of _sum and
//  the sum's of the thread-local parts of the counter.
// Representation invariant: Every thread-local part is in the linked
//  list of the thread-local parts of its counter, as well as in the
//  linked list of the counters of a the thread.
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
// Key creation primivites.
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
// Doubly-linked list primitives.  In an ideal world these would be
//  parameterized and put into a separate library.  Perhaps STL even has them,
//  but I (bradley) don't see how to make the STL linked lists avoid an extra
//  malloc for the list cell.  This linked list allows you to embed several
//  linked lists into a single object.
//******************************************************************************


struct linked_list_element {
    struct local_counter *container;         // The container is to avoid the offset nonsense in the toku linked list.
    //                                       // The container points to the object in which we are embedded.
    struct linked_list_element *next, *prev; // prev==NULL for the first element of the list.  next==NULL for the last element.
};

static void ll_insert (struct linked_list_head *head, struct linked_list_element *item, struct local_counter *container) 
// Effect: Add an item to a linked list.
// Implementation note: Push the item to the head of the list.
{
    struct linked_list_element *old_first = head->first;
    item->container = container;
    item->next      = old_first;
    item->prev      = NULL;
    if (old_first) {
        old_first->prev = item;
    }
    head->first = item;
}

static void ll_remove (struct linked_list_head *head, struct linked_list_element *item) 
// Effect: Remove an item from a linked list.
// Requires: The item is in the list identified by head.
{
    struct linked_list_element *old_prev = item->prev;
    struct linked_list_element *old_next = item->next;
    if (old_prev==NULL) {
        head->first = old_next;
    } else {
        old_prev->next = old_next;
    }
    if (item->next==NULL) {
        /* nothing */
    } else {
        old_next->prev = old_prev;
    }
}

static bool ll_pop (struct linked_list_head *head, struct linked_list_element **item) 
// Effect: if head is an empty list return false.
//   Otherwise return true and set *item to the first item, and remove that item from the list.
{
    struct linked_list_element *first = head->first;
    if (first) {
        assert(first->prev==NULL);
        head->first = first->next;
        if (first->next) {
            first->next->prev=NULL;
        }
        first->next=NULL;
        *item = first;
        return true;
    } else {
        return false;
    }
}

//******************************************************************************
// The thread local part of a counter, comprising the thread-local sum a pointer
//  to the partitioned_counter, a pointer to the thread_local list head, and two
//  linked lists. One of the lists is all the thread-local parts that belong to
//  the same counter, and the other is all the thread-local parts that belogn to
//  the same thread.
//******************************************************************************

struct local_counter {
    u_int64_t                  sum;           // The thread-local sum.
    PARTITIONED_COUNTER       *owner_pc;      // The partitioned counter that this is part of.
    struct linked_list_head   *thread_head;   // The head of the list of all the counters for a thread.
    struct linked_list_element ll_in_counter; // Linked list elements for the doubly-linked list of thread-local information for the same PARTITIONED_COUNTER.
    struct linked_list_element ll_in_thread;  // Linked list elements for the doubly-linked list of all the local parts of counters for the same thread.
};

// The head of the ll_in_thread linked list for each thread is stored in this thread-local variable.
__thread struct linked_list_head ll_thread_head = {NULL};

// I want this to be static, but I have to use hidden visibility instead because it's a friend function.
void destroy_thread_local_part_of_partitioned_counters (void *ignore_me) __attribute__((__visibility__("hidden")));
void destroy_thread_local_part_of_partitioned_counters (void *ignore_me __attribute__((__unused__)))
// Effect: This function is called whenever a thread terminates using the
//  destructor of the thread_destructor_key (defined below).  First grab the
//  lock, then go through all the partitioned counters and removes the part that
//  is local to this thread.  We don't actually need the contents of the
//  thread_destructor_key except to cause this function to run.  The content of
//  the key is a static string, so don't try to free it.
{
    pc_lock();
    struct linked_list_element *le;
    // The ll_thread_head variable is still present even while running
    // pthread_key destructors.  I tried to use a different pthread_key, but it
    // turned out that the other one was being destroyed before this one gets to
    // run.  So I'm using a __thread variable to get at the head of the linked
    // list.
    while (ll_pop(&ll_thread_head, &le)) {
        struct local_counter *lc = le->container;
        // We just removed lc from the list from the thread.
        // Remove lc from the partitioned counter in which it resides.
        PARTITIONED_COUNTER *owner = lc->owner_pc;
        owner->_sum_of_dead += lc->sum;
        ll_remove(&owner->_ll_counter_head, &lc->ll_in_counter);
        toku_free(lc);
    }
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
static void initiate_partitioned_counters (void) __attribute__((__constructor__));
static void initiate_partitioned_counters (void)
// Effect: This constructor function runs before any of the other code here, and
//  sets up a pthread_key with a destructor.
{
    pk_create(&thread_destructor_key, destroy_thread_local_part_of_partitioned_counters);
}

PARTITIONED_COUNTER::PARTITIONED_COUNTER(void) 
// Effect: Create a counter, initialized to zero.
{
    _sum_of_dead = 0;
    pk_create(&_key, NULL);
    _ll_counter_head.first = NULL;
}

PARTITIONED_COUNTER::~PARTITIONED_COUNTER(void)
// Effect: Destroy the counter.  No operations on this counter are permitted after.
// Implementation note: Since we have a global lock, we can destroy all the key-specific versions as well.
{
    pk_delete(_key);
    pc_lock();
    struct linked_list_element *first;
    while (ll_pop(&_ll_counter_head, &first)) {
        // We just removed first from the counter list, now we must remove it from the thread head
        struct local_counter *lc = first->container;
        ll_remove(lc->thread_head, &lc->ll_in_thread);
        toku_free(first->container);
    }
    pc_unlock();
}

void PARTITIONED_COUNTER::increment(u_int64_t amount)
// Effect: Increment the counter by amount.
// Requires: No overflows.  This is a 64-bit unsigned counter.
{
    struct local_counter *CAST_FROM_VOIDP(lc, pthread_getspecific(_key));
    if (lc==NULL) {
	XMALLOC(lc);
	lc->sum         = 0;
	HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&lc->sum, sizeof(lc->sum)); // the counter increment is kind of racy.
	lc->owner_pc    = this;
        lc->thread_head = &ll_thread_head;
        pk_setspecific(_key, lc);

        // Set things up so that this thread terminates, the thread-local parts of the counter will be destroyed and merged into their respective counters.
        pk_setspecific(thread_destructor_key, "dont care");

        // Of all the code in this method, only the following two
        // operations must be done with the lock held.  The
        // setspecific operations don't matter because the pthread_key
        // accessors and destructors are run by the same thread.  The
        // other contents of *lc are similarly thread-local.

        // It probably doesn't really matter since this increment is relatively infrequent.

	pc_lock(); 
        ll_insert(&_ll_counter_head, &lc->ll_in_counter, lc);
        ll_insert(&ll_thread_head,   &lc->ll_in_thread,  lc); // We do have to hold the lock for this insert because the thread destructor may access
        //                                                    // ll_thread_head through some ll_remove(lc->thread_head,...) operation in the
        //                                                    // partitioned_counter destructor.
	pc_unlock();
    }
    lc->sum += amount;
}

u_int64_t PARTITIONED_COUNTER::read(void)
// Effect: Return the current value of the counter.
// Implementation note: Sum all the thread-local counts along with the sum_of_the_dead.
{
    pc_lock();
    u_int64_t sum = _sum_of_dead;
    for (struct linked_list_element *le = _ll_counter_head.first; le; le=le->next) {
	sum += le->container->sum;
    }
    pc_unlock();
    return sum;
}
