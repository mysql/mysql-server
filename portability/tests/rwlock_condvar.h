/* Fair readers writer lock implemented using condition variables.
 * This is maintained so that we can measure the performance of a relatively simple implementation (this one) 
 * compared to a fast one that uses compare-and-swap (the one in ../toku_rwlock.c)
 */

#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."

#include <stdbool.h>

// Fair readers/writer locks.  These are fair (meaning first-come first-served.  No reader starvation, and no writer starvation).  And they are
// probably faster than the linux readers/writer locks (pthread_rwlock_t).
struct toku_cv_fair_rwlock_waiter_state; // this structure is used internally.
typedef struct toku_cv_fair_rwlock_s {
    toku_mutex_t                  mutex;
    int                                   state; // 0 means no locks, + is number of readers locked, -1 is a writer
    struct toku_cv_fair_rwlock_waiter_state *waiters_head, *waiters_tail;
} toku_cv_fair_rwlock_t;

int toku_cv_fair_rwlock_init (toku_cv_fair_rwlock_t *rwlock);
int toku_cv_fair_rwlock_destroy (toku_cv_fair_rwlock_t *rwlock);
int toku_cv_fair_rwlock_rdlock (toku_cv_fair_rwlock_t *rwlock);
int toku_cv_fair_rwlock_wrlock (toku_cv_fair_rwlock_t *rwlock);
int toku_cv_fair_rwlock_unlock (toku_cv_fair_rwlock_t *rwlock);

struct toku_cv_fair_rwlock_waiter_state {
    char is_read;
    struct toku_cv_fair_rwlock_waiter_state *next;
    toku_cond_t cond;
};

#if defined(HAVE_GNU_TLS)
static __thread struct toku_cv_fair_rwlock_waiter_state waitstate_var = {0, NULL, { PTHREAD_COND_INITIALIZER } };
#define GET_WAITSTATE(name) name = &waitstate_var
#else
#include <memory.h>
static pthread_key_t waitstate_key;
static bool waitstate_key_initialized = false;

void
toku_rwlock_init(void)
{
    assert(!waitstate_key_initialized);
    int r = toku_pthread_key_create(&waitstate_key, toku_free);
    assert_zero(r);
    waitstate_key_initialized = true;
}

void
toku_rwlock_destroy(void)
{
    assert(waitstate_key_initialized);
    int r = toku_pthread_key_delete(waitstate_key);
    assert_zero(r);
    waitstate_key_initialized = false;
}

static struct toku_cv_fair_rwlock_waiter_state *
get_waitstate(void)
{
    assert(waitstate_key_initialized);
    struct toku_cv_fair_rwlock_waiter_state *waitstate = NULL;
    void *p = toku_pthread_getspecific(waitstate_key);
    if (!p) {
        p = toku_xmalloc(sizeof *waitstate);
        int r = toku_pthread_setspecific(waitstate_key, p);
        assert_zero(r);
    }
    waitstate = p;
    return waitstate;
}

#define GET_WAITSTATE(name) name = get_waitstate()
#endif

int toku_cv_fair_rwlock_init (toku_cv_fair_rwlock_t *rwlock) {
    rwlock->state=0;
    rwlock->waiters_head = NULL;
    rwlock->waiters_tail = NULL;
    toku_mutex_init(&rwlock->mutex, NULL);
    return 0;
}

int toku_cv_fair_rwlock_destroy (toku_cv_fair_rwlock_t *rwlock) {
    toku_mutex_destroy(&rwlock->mutex);
    return 0;
}

int toku_cv_fair_rwlock_rdlock (toku_cv_fair_rwlock_t *rwlock) {
    struct toku_cv_fair_rwlock_waiter_state *GET_WAITSTATE(waitstate);
    toku_mutex_lock(&rwlock->mutex);
    if (rwlock->waiters_head!=NULL || rwlock->state<0) {
	// Someone is ahead of me in the queue, or someone has a lock.
	// We use per-thread-state for the condition variable.  A thread cannot get control and try to reuse the waiter state for something else.
	if (rwlock->waiters_tail) {
	    rwlock->waiters_tail->next = waitstate;
	} else {
	    rwlock->waiters_head = waitstate;
	}
	rwlock->waiters_tail = waitstate;
	waitstate->next = NULL;
	waitstate->is_read = 1; 
	do {
	    toku_cond_wait(&waitstate->cond, &rwlock->mutex);
	} while (rwlock->waiters_head!=waitstate || rwlock->state<0);
	rwlock->state++;
	rwlock->waiters_head=waitstate->next;
	if (waitstate->next==NULL) rwlock->waiters_tail=NULL;
	if (rwlock->waiters_head && rwlock->waiters_head->is_read) {
	    toku_cond_signal(&rwlock->waiters_head->cond);
	}
    } else {
	// No one is waiting, and any holders are readers.
	rwlock->state++;
    }
    toku_mutex_unlock(&rwlock->mutex);
    return 0;
}

int toku_cv_fair_rwlock_wrlock (toku_cv_fair_rwlock_t *rwlock) {
    struct toku_cv_fair_rwlock_waiter_state *GET_WAITSTATE(waitstate);
    toku_mutex_lock(&rwlock->mutex);
    if (rwlock->waiters_head!=NULL || rwlock->state!=0) {
	// Someone else is ahead of me, or someone has a lock the lock, so we must wait our turn.
	if (rwlock->waiters_tail) {
	    rwlock->waiters_tail->next = waitstate;
	} else {
	    rwlock->waiters_head = waitstate;
	}
	rwlock->waiters_tail = waitstate;
	waitstate->next = NULL;
	waitstate->is_read = 0;
	do {
	    toku_cond_wait(&waitstate->cond, &rwlock->mutex);
	} while (rwlock->waiters_head!=waitstate || rwlock->state!=0);
	rwlock->waiters_head = waitstate->next;
	if (waitstate->next==NULL) rwlock->waiters_tail=NULL;
    }
    rwlock->state = -1;
    toku_mutex_unlock(&rwlock->mutex);
    return 0;
}

int toku_cv_fair_rwlock_unlock (toku_cv_fair_rwlock_t *rwlock) {
    toku_mutex_lock(&rwlock->mutex);
    assert(rwlock->state!=0);
    if (rwlock->state>0) {
	rwlock->state--;
    } else {
	rwlock->state=0;
    }
    if (rwlock->state==0 && rwlock->waiters_head) {
	toku_cond_signal(&rwlock->waiters_head->cond);
    } else {
	// printf(" No one to wake\n");
    }
    toku_mutex_unlock(&rwlock->mutex);
    return 0;
}

