/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."
#ident "$Id:$"

#include <errno.h>
#include <string.h>

#include "toku_assert.h"
#include "brttypes.h"
#include "minicron.h"
#include "toku_portability.h"

static void
toku_gettime (struct timespec *a) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    a->tv_sec  = tv.tv_sec;
    a->tv_nsec = tv.tv_usec * 1000LL;
}
    

static int
timespec_compare (struct timespec *a, struct timespec *b) {
    if (a->tv_sec > b->tv_sec) return 1;
    if (a->tv_sec < b->tv_sec) return -1;
    if (a->tv_nsec > b->tv_nsec) return 1;
    if (a->tv_nsec < b->tv_nsec) return -1;
    return 0;
}

// Implementation notes:
//  When calling do_shutdown or change_period, the mutex is obtained, the variables in the minicron struct are modified, and
//  the condition variable is signalled.  Possibly the minicron thread will miss the signal.  To avoid this problem, whenever
//  the minicron thread acquires the mutex, it must check to see what the variables say to do (e.g., should it shut down?).

static void*
minicron_do (void *pv)
{
    struct minicron *p = pv;
    int r = toku_pthread_mutex_lock(&p->mutex);
    assert(r==0);
    while (1) {
	if (p->do_shutdown) {
	    r = toku_pthread_mutex_unlock(&p->mutex);
	    assert(r==0);
	    return 0;
	}
	if (p->period_in_seconds==0) {
	    // if we aren't supposed to do it then just do an untimed wait.
	    r = toku_pthread_cond_wait(&p->condvar, &p->mutex);
	    assert(r==0);
	} else {
	    // Recompute the wakeup time every time (instead of once per call to f) in case the period changges.
	    struct timespec wakeup_at = p->time_of_last_call_to_f;
	    wakeup_at.tv_sec += p->period_in_seconds;
	    struct timespec now;
	    toku_gettime(&now);
	    //printf("wakeup at %.6f (after %d seconds) now=%.6f\n", wakeup_at.tv_sec + wakeup_at.tv_nsec*1e-9, p->period_in_seconds, now.tv_sec + now.tv_nsec*1e-9);
	    r = toku_pthread_cond_timedwait(&p->condvar, &p->mutex, &wakeup_at);
	    if (r!=0 && r!=ETIMEDOUT) fprintf(stderr, "%s:%d r=%d (%s)", __FILE__, __LINE__, r, strerror(r));
	    assert(r==0 || r==ETIMEDOUT);
	}
	// Now we woke up, and we should figure out what to do
	if (p->do_shutdown) {
	    r = toku_pthread_mutex_unlock(&p->mutex);
	    assert(r==0);
	    return 0;
	}
	if (p->period_in_seconds >0) {
	    // maybe do a checkpoint
	    struct timespec now;
	    toku_gettime(&now);
	    struct timespec time_to_call = p->time_of_last_call_to_f;
	    time_to_call.tv_sec += p->period_in_seconds;
	    int compare = timespec_compare(&time_to_call, &now);
	    //printf("compare(%.6f, %.6f)=%d\n", time_to_call.tv_sec + time_to_call.tv_nsec*1e-9, now.tv_sec+now.tv_nsec*1e-9, compare);
	    if (compare <= 0) {
		r = toku_pthread_mutex_unlock(&p->mutex);
		assert(r==0);
		r = p->f(p->arg);
		assert(r==0);
		r = toku_pthread_mutex_lock(&p->mutex);
		assert(r==0);
		toku_gettime(&p->time_of_last_call_to_f); // the period is measured between calls to f.
		
	    }
	}
    }
}

int
toku_minicron_setup(struct minicron *p, u_int32_t period_in_seconds, int(*f)(void *), void *arg)
{
    p->f = f;
    p->arg = arg;
    toku_gettime(&p->time_of_last_call_to_f);
    //printf("now=%.6f", p->time_of_last_call_to_f.tv_sec + p->time_of_last_call_to_f.tv_nsec*1e-9);
    p->period_in_seconds = period_in_seconds; 
    p->do_shutdown = FALSE;
    { int r = toku_pthread_mutex_init(&p->mutex, 0);   assert(r==0); }
    { int r = toku_pthread_cond_init (&p->condvar, 0); assert(r==0); }
    //printf("%s:%d setup period=%d\n", __FILE__, __LINE__, period_in_seconds);
    return toku_pthread_create(&p->thread, 0, minicron_do, p);
}
    
int
toku_minicron_change_period(struct minicron *p, u_int32_t new_period)
{
    int r = toku_pthread_mutex_lock(&p->mutex);   assert(r==0);
    p->period_in_seconds = new_period;
    r = toku_pthread_cond_signal(&p->condvar);    assert(r==0);
    r = toku_pthread_mutex_unlock(&p->mutex);     assert(r==0);
    return 0;
}

int
toku_minicron_shutdown(struct minicron *p) {
    int r = toku_pthread_mutex_lock(&p->mutex);        assert(r==0);
    assert(!p->do_shutdown);
    p->do_shutdown = TRUE;
    //printf("%s:%d signalling\n", __FILE__, __LINE__);
    r = toku_pthread_cond_signal(&p->condvar);         assert(r==0);
    r = toku_pthread_mutex_unlock(&p->mutex);          assert(r==0);
    void *returned_value;
    //printf("%s:%d joining\n", __FILE__, __LINE__);
    r = toku_pthread_join(p->thread, &returned_value);
    if (r!=0) fprintf(stderr, "%s:%d r=%d (%s)\n", __FILE__, __LINE__, r, strerror(r));
    assert(r==0);  assert(returned_value==0);
    r = toku_pthread_cond_destroy(&p->condvar);        assert(r==0);
    r = toku_pthread_mutex_destroy(&p->mutex);         assert(r==0);
    //printf("%s:%d shutdowned\n", __FILE__, __LINE__);
    return 0;
}

BOOL
toku_minicron_has_been_shutdown(struct minicron *p) {
    return p->do_shutdown;
}
