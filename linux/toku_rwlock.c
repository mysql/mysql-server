/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."

#include <pthread.h>
#include <toku_assert.h>
#include "toku_pthread.h"

struct toku_fair_rwlock_waiter_state {
    char is_read;
    struct toku_fair_rwlock_waiter_state *next;
    pthread_cond_t cond;
};

static __thread struct toku_fair_rwlock_waiter_state waitstate = {0, NULL, PTHREAD_COND_INITIALIZER };

int toku_fair_rwlock_init (toku_fair_rwlock_t *rwlock) {
    rwlock->state=0;
    rwlock->waiters_head = NULL;
    rwlock->waiters_tail = NULL;
    return toku_pthread_mutex_init(&rwlock->mutex, NULL);
}

int toku_fair_rwlock_destroy (toku_fair_rwlock_t *rwlock) {
    return toku_pthread_mutex_destroy(&rwlock->mutex);
}

int toku_fair_rwlock_rdlock (toku_fair_rwlock_t *rwlock) {
    int r = toku_pthread_mutex_lock(&rwlock->mutex);
    assert(r==0);
    if (rwlock->waiters_head!=NULL || rwlock->state<0) {
	// Someone is ahead of me in the queue, or someone has a lock.
	// We use per-thread-state for the condition variable.  A thread cannot get control and try to reuse the waiter state for something else.
	if (rwlock->waiters_tail) {
	    rwlock->waiters_tail->next = &waitstate;
	} else {
	    rwlock->waiters_head = &waitstate;
	}
	rwlock->waiters_tail = &waitstate;
	waitstate.next = NULL;
	waitstate.is_read = 1; 
	do {
	    r = toku_pthread_cond_wait(&waitstate.cond, &rwlock->mutex);
	    assert(r==0);
	} while (rwlock->waiters_head!=&waitstate || rwlock->state<0);
	rwlock->state++;
	rwlock->waiters_head=waitstate.next;
	if (waitstate.next==NULL) rwlock->waiters_tail=NULL;
	if (rwlock->waiters_head && rwlock->waiters_head->is_read) {
	    r = toku_pthread_cond_signal(&rwlock->waiters_head->cond);
	    assert(r==0);
	}
    } else {
	// No one is waiting, and any holders are readers.
	rwlock->state++;
    }
    r = toku_pthread_mutex_unlock(&rwlock->mutex);
    assert(r==0);
    return 0;
}

int toku_fair_rwlock_wrlock (toku_fair_rwlock_t *rwlock) {
    int r = toku_pthread_mutex_lock(&rwlock->mutex);
    assert(r==0);
    if (rwlock->waiters_head!=NULL || rwlock->state!=0) {
	// Someone else is ahead of me, or someone has a lock the lock, so we must wait our turn.
	if (rwlock->waiters_tail) {
	    rwlock->waiters_tail->next = &waitstate;
	} else {
	    rwlock->waiters_head = &waitstate;
	}
	rwlock->waiters_tail = &waitstate;
	waitstate.next = NULL;
	waitstate.is_read = 0;
	do {
	    r = toku_pthread_cond_wait(&waitstate.cond, &rwlock->mutex);
	    assert(r==0);
	} while (rwlock->waiters_head!=&waitstate || rwlock->state!=0);
	rwlock->waiters_head = waitstate.next;
	if (waitstate.next==NULL) rwlock->waiters_tail=NULL;
    }
    rwlock->state = -1;
    r = toku_pthread_mutex_unlock(&rwlock->mutex);
    assert(r==0);
    return 0;
}

int toku_fair_rwlock_unlock (toku_fair_rwlock_t *rwlock) {
    int r = toku_pthread_mutex_lock(&rwlock->mutex);
    assert(r==0);
    assert(rwlock->state!=0);
    if (rwlock->state>0) {
	rwlock->state--;
    } else {
	rwlock->state=0;
    }
    if (rwlock->state==0 && rwlock->waiters_head) {
	r = toku_pthread_cond_signal(&rwlock->waiters_head->cond);
	assert(r==0);
    } else {
	// printf(" No one to wake\n");
    }
    r = toku_pthread_mutex_unlock(&rwlock->mutex);
    assert(r==0);
    return 0;
}

