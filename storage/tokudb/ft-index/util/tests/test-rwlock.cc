/* -*- mode: C; c-basic-offset: 4 -*- */

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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."

// Here are some timing numbers:
// (Note: The not-quite-working version with cas can be found in r22519 of https://svn.tokutek.com/tokudb/toku/tokudb.2825/)  It's about as fast as "Best cas".)
//
// On ramie (2.53GHz E5540)
//  Best nop           time=  1.074300ns
//  Best cas           time=  8.595600ns
//  Best mutex         time= 19.340201ns
//  Best rwlock        time= 34.024799ns
//  Best newbrt rwlock time= 38.680500ns
//  Best prelocked     time=  2.148700ns
//  Best fair rwlock   time= 45.127600ns
// On laptop
//  Best nop           time=  2.876000ns
//  Best cas           time= 15.362500ns
//  Best mutex         time= 51.951498ns
//  Best rwlock        time= 97.721201ns
//  Best newbrt rwlock time=110.456800ns
//  Best prelocked     time=  4.240100ns
//  Best fair rwlock   time=113.119102ns
//
// Analysis:  If the mutex can be prelocked (as cachetable does, it uses the same mutex for the cachetable and for the condition variable protecting the cache table)
//  then you can save quite a bit.  What does the cachetable do?
//  During pin:   (In the common case:) It grabs the mutex, grabs a read lock,  and releases the mutex.
//  During unpin: It grabs the mutex, unlocks the rwlock lock in the pair, and releases the mutex. 
//  Both actions must acquire a cachetable lock during that time, so definitely saves time to do it that way.

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include <toku_portability.h>
#include <toku_assert.h>
#include <portability/toku_atomic.h>
#include <portability/toku_fair_rwlock.h>
#include <portability/toku_pthread.h>
#include <portability/toku_time.h>
#include <util/frwlock.h>
#include <util/rwlock.h>
#include "rwlock_condvar.h"

static int verbose=1;
static int timing_only=0;

static void parse_args (int argc, const char *argv[]) {
    const char *progname = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0], "-q")==0) {
	    verbose--;
	} else if (strcmp(argv[0], "--timing-only")==0) {
	    timing_only=1;
	} else {
	    fprintf(stderr, "Usage: %s {-q}* {-v}* {--timing-only}\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
}

static const int T=6;
static const int N=10000000;

static double best_nop_time=1e12;
static double best_fcall_time=1e12;
static double best_cas_time=1e12;
static double best_mutex_time=1e12;
static double best_rwlock_time=1e12;
static double best_newbrt_time=1e12;
static double best_prelocked_time=1e12;
static double best_cv_fair_rwlock_time=1e12; // fair from condition variables
static double best_fair_rwlock_time=1e12;
static double best_frwlock_time=1e12;
static double best_frwlock_prelocked_time=1e12;
static double mind(double a, double b) { if (a<b) return a; else return b; }

#if 0
// gcc 4.4.4 (fedora 12) doesn't introduce memory barriers on these writes, so I think that volatile is not enough for sequential consistency.
// Intel guarantees that writes are seen in the same order as they were performed on one processor.  But if there were two processors, funny things could happen.
volatile int sc_a, sc_b;
void sequential_consistency (void) {
    sc_a = 1;
    sc_b = 0;
}
#endif
    
// Declaring val to be volatile produces essentially identical code as putting the asm volatile memory statements in.
// gcc is not introducing memory barriers to force sequential consistency on volatile memory writes.
// That's probably good enough for us, since we'll have a barrier instruction anywhere it matters.
volatile int val = 0;

static void time_nop (void) __attribute((__noinline__)); // don't want it inline, because it messes up timing.
static void time_nop (void) {
    struct timeval start,end;
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
	for (int i=0; i<N; i++) {
	    if (val!=0) abort();
	    val=1;
	    //__asm__ volatile ("" : : : "memory");
	    val=0;
	    //__asm__ volatile ("" : : : "memory");
	}
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "nop               = %.6fns/(lock+unlock)\n", diff);
	best_nop_time=mind(best_nop_time,diff);
    }
}

// This function is defined so we can measure the cost of a function call.
int fcall_nop (int i) __attribute__((__noinline__));
int fcall_nop (int i) {
    return i;
}

void time_fcall (void) __attribute((__noinline__));
void time_fcall (void) {
    struct timeval start,end;
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
	for (int i=0; i<N; i++) {
	    fcall_nop(i);
	}
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "fcall             = %.6fns/(lock+unlock)\n", diff);
	best_fcall_time=mind(best_fcall_time,diff);
    }
}

void time_cas (void) __attribute__((__noinline__));
void time_cas (void) {
    volatile int64_t tval = 0;
    struct timeval start,end;
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
	for (int i=0; i<N; i++) {
	    { int r = toku_sync_val_compare_and_swap(&tval, 0, 1);  assert(r==0); }
	    { int r = toku_sync_val_compare_and_swap(&tval, 1, 0);  assert(r==1); }
	}
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "cas           = %.6fns/(lock+unlock)\n", diff);
	best_cas_time=mind(best_cas_time,diff);
    }
}


void time_pthread_mutex (void) __attribute__((__noinline__));
void time_pthread_mutex (void) {
    pthread_mutex_t mutex;
    { int r = pthread_mutex_init(&mutex, NULL); assert(r==0); }
    struct timeval start,end;
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
	for (int i=0; i<N; i++) {
	    pthread_mutex_lock(&mutex);
	    pthread_mutex_unlock(&mutex);
	}
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "pthread_mutex     = %.6fns/(lock+unlock)\n", diff);
	best_mutex_time=mind(best_mutex_time,diff);
    }
    { int r = pthread_mutex_destroy(&mutex);    assert(r==0); }
}

void time_pthread_rwlock (void) __attribute__((__noinline__));
void time_pthread_rwlock (void) {
    pthread_rwlock_t mutex;
    { int r = pthread_rwlock_init(&mutex, NULL); assert(r==0); }
    struct timeval start,end;
    pthread_rwlock_rdlock(&mutex);
    pthread_rwlock_unlock(&mutex);
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
	for (int i=0; i<N; i++) {
	    pthread_rwlock_rdlock(&mutex);
	    pthread_rwlock_unlock(&mutex);
	}
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "pthread_rwlock(r) = %.6fns/(lock+unlock)\n", diff);
	best_rwlock_time=mind(best_rwlock_time,diff);
    }
    { int r = pthread_rwlock_destroy(&mutex);    assert(r==0); }
}

static void newbrt_rwlock_lock (RWLOCK rwlock, toku_mutex_t *mutex) {
    toku_mutex_lock(mutex);
    rwlock_read_lock(rwlock, mutex);
    toku_mutex_unlock(mutex);
}

static void newbrt_rwlock_unlock (RWLOCK rwlock, toku_mutex_t *mutex) {
    toku_mutex_lock(mutex);
    rwlock_read_unlock(rwlock);
    toku_mutex_unlock(mutex);
}

// Time the read lock that's in newbrt/rwlock.h
void time_newbrt_rwlock (void) __attribute((__noinline__));
void time_newbrt_rwlock (void) {
    struct rwlock rwlock;
    toku_mutex_t external_mutex;
    toku_mutex_init(&external_mutex, NULL);
    rwlock_init(&rwlock);
    struct timeval start,end;
    
    newbrt_rwlock_lock(&rwlock, &external_mutex);
    newbrt_rwlock_unlock(&rwlock, &external_mutex);
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
	for (int i=0; i<N; i++) {
	    newbrt_rwlock_lock(&rwlock, &external_mutex);
	    newbrt_rwlock_unlock(&rwlock, &external_mutex);
	}
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "newbrt_rwlock(r) = %.6fns/(lock+unlock)\n", diff);
	best_newbrt_time=mind(best_newbrt_time,diff);
    }
    rwlock_destroy(&rwlock);
    toku_mutex_destroy(&external_mutex);
}

// Time the read lock that's in newbrt/rwlock.h, assuming the mutex is already held.
void time_newbrt_prelocked_rwlock (void) __attribute__((__noinline__));
void time_newbrt_prelocked_rwlock (void) {
    struct rwlock rwlock;
    toku_mutex_t external_mutex;
    toku_mutex_init(&external_mutex, NULL);
    toku_mutex_lock(&external_mutex);
    rwlock_init(&rwlock);
    struct timeval start,end;
    
    rwlock_read_lock(&rwlock, &external_mutex);
    rwlock_read_unlock(&rwlock);
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
	for (int i=0; i<N; i++) {
	    rwlock_read_lock(&rwlock, &external_mutex);
	    rwlock_read_unlock(&rwlock);
	}
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "pre_newbrt_rwlock(r) = %.6fns/(lock+unlock)\n", diff);
	best_prelocked_time=mind(best_prelocked_time,diff);
    }
    rwlock_destroy(&rwlock);
    toku_mutex_unlock(&external_mutex);
    toku_mutex_destroy(&external_mutex);
}

void time_toku_fair_rwlock (void) __attribute__((__noinline__));
void time_toku_fair_rwlock (void) {
    toku_fair_rwlock_t mutex;
    toku_fair_rwlock_init(&mutex);
    struct timeval start,end;
    toku_fair_rwlock_rdlock(&mutex);
    toku_fair_rwlock_unlock(&mutex);
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
	for (int i=0; i<N; i++) {
	    toku_fair_rwlock_rdlock(&mutex);
	    toku_fair_rwlock_unlock(&mutex);
	}
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "pthread_fair(r)   = %.6fns/(lock+unlock)\n", diff);
	best_fair_rwlock_time=mind(best_fair_rwlock_time,diff);
    }
    toku_fair_rwlock_destroy(&mutex);
}

/* not static*/
void time_toku_cv_fair_rwlock(void) __attribute__((__noinline__));
void time_toku_cv_fair_rwlock(void) {
    toku_cv_fair_rwlock_t mutex;
    toku_cv_fair_rwlock_init(&mutex);
    struct timeval start,end;
    toku_cv_fair_rwlock_rdlock(&mutex);
    toku_cv_fair_rwlock_unlock(&mutex);
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
	for (int i=0; i<N; i++) {
	    toku_cv_fair_rwlock_rdlock(&mutex);
	    toku_cv_fair_rwlock_unlock(&mutex);
	}
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "pthread_cvfair(r) = %.6fns/(lock+unlock)\n", diff);
	best_cv_fair_rwlock_time=mind(best_cv_fair_rwlock_time,diff);
    }
    toku_cv_fair_rwlock_destroy(&mutex);
}

void time_frwlock_prelocked(void) __attribute__((__noinline__));
void time_frwlock_prelocked(void) {
    toku_mutex_t external_mutex;
    toku_mutex_init(&external_mutex, NULL);
    struct timeval start,end;
    toku::frwlock x;
    x.init(&external_mutex);
    toku_mutex_lock(&external_mutex);
    bool got_lock;
    x.read_lock();
    x.read_unlock();

    got_lock = x.try_read_lock();
    invariant(got_lock);
    x.read_unlock();
    x.write_lock(true);
    x.write_unlock();
    got_lock = x.try_write_lock(true);
    invariant(got_lock);
    x.write_unlock();
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
	for (int i=0; i<N; i++) {
	    x.read_lock();
	    x.read_unlock();
	}
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "frwlock_prelocked = %.6fns/(lock+unlock)\n", diff);
        best_frwlock_prelocked_time=mind(best_frwlock_prelocked_time,diff);
    }
    x.deinit();
    toku_mutex_unlock(&external_mutex);
    toku_mutex_destroy(&external_mutex);
}

void time_frwlock(void) __attribute__((__noinline__));
void time_frwlock(void) {
    toku_mutex_t external_mutex;
    toku_mutex_init(&external_mutex, NULL);
    struct timeval start,end;
    toku::frwlock x;
    x.init(&external_mutex);
    toku_mutex_lock(&external_mutex);
    x.read_lock();
    x.read_unlock();
    toku_mutex_unlock(&external_mutex);
    for (int t=0; t<T; t++) {
	gettimeofday(&start, NULL);
        for (int i=0; i<N; i++) {
            toku_mutex_lock(&external_mutex);
            x.read_lock();
            toku_mutex_unlock(&external_mutex);

            toku_mutex_lock(&external_mutex);
            x.read_unlock();
            toku_mutex_unlock(&external_mutex);
        }
	gettimeofday(&end,   NULL);
	double diff = 1e9*toku_tdiff(&end, &start)/N;
	if (verbose>1)
	    fprintf(stderr, "frwlock           = %.6fns/(lock+unlock)\n", diff);
        best_frwlock_time=mind(best_frwlock_time,diff);
    }
    x.deinit();
    toku_mutex_destroy(&external_mutex);
}


#define N 6
#define T 150000
#define L 5
#define N_LOG_ENTRIES (L*N*4)

static toku_fair_rwlock_t rwlock;

static struct log_s {
    int threadid, loopid;
    char action;
} actionlog[N_LOG_ENTRIES];
static int log_counter=0;

static void logit (int threadid, int loopid, char action) {
    //printf("%d %d %c\n", threadid, loopid, action);
    int my_log_counter = toku_sync_fetch_and_add(&log_counter, 1);
    assert(my_log_counter<N_LOG_ENTRIES);
    actionlog[my_log_counter].threadid = threadid;
    actionlog[my_log_counter].loopid   = loopid;
    actionlog[my_log_counter].action   = action;
}

// The action should look like this:
//   Threads 0-2 are reader threads.
//   Threads 3-6 are writer threads.
// The threads all repeatedly grab the lock, wait T steps, and release.
// If the readers can starve the writers, then most of the writers will be at the end.
// If the writers can starve the readers, then most of the readers will be at the end.
// The reader threads all grab the lock, wait T*2 steps, and release the lock.
// The writer threads
// First the writer threads wait time T while the reader threads all go for the lock.
// Before the first one lets go, the writer threads wake up and try to grab the lock.  But the readers are still 

//   3 threads (0-2) try to grab the lock all at once.  They'll get it.  They each sleep for time T*2
//   3 threads (3-6) try to grab the write lock.  They'll get it one after another.


extern __thread int mytid;

static void grab_rdlock (int threadid, int iteration) {
    logit(threadid, iteration, 't');
    { int r = toku_fair_rwlock_rdlock(&rwlock); assert(r==0); }
    logit(threadid, iteration, 'R');
}

static void release_rdlock (int threadid, int iteration) {
    logit(threadid, iteration, 'u');
    { int r = toku_fair_rwlock_unlock(&rwlock); assert(r==0); }
}

static void grab_wrlock (int threadid, int iteration) {
    logit(threadid, iteration, 'T');
    { int r = toku_fair_rwlock_wrlock(&rwlock); assert(r==0); }
    logit(threadid, iteration, 'W');
}

static void release_wrlock (int threadid, int iteration) {
    logit(threadid, iteration, 'U');
    { int r = toku_fair_rwlock_unlock(&rwlock); assert(r==0);}
}

static void *start_thread (void *vv) {
    int *vp=(int*)vv;
    int v=*vp;

    //printf("T%d=%ld\n", v, pthread_self());
    switch(v) {
    case 0:
    case 1:
    case 2:
	for (int i=0; i<L; i++) {
	    grab_rdlock(v, i);
	    usleep(T);
	    release_rdlock(v, i);
	}
	break;
    case 3:
    case 4:
    case 5:
	for (int i=0; i<L; i++) {
	    grab_wrlock(v, i);
	    usleep(T);
	    release_wrlock(v, i);
	}
    }
    return NULL;
}

static void *start_thread_random (void *vv) {
    int *vp=(int*)vv;
    int v=*vp;

    for (int i=0; i<L; i++) {
	if (random()%2==0) {
	    grab_rdlock(v, i);
	    for (int j=0; j<random()%20; j++) sched_yield();
	    release_rdlock(v, i);
	    for (int j=0; j<random()%20; j++) sched_yield();
	} else {
	    grab_wrlock(v, i);
	    for (int j=0; j<random()%20; j++) sched_yield();
	    release_wrlock(v, i);
	    for (int j=0; j<random()%20; j++) sched_yield();
	}
    }
    return NULL;
}

static void check_actionlog (int expected_writer_max_count,
			     int expected_reader_parallelism_min,
			     int expected_reader_parallelism_max)
// Effect:
//  Make sure that writers are exclusive.
//  Make sure that anyone who asks for a lock doesn't have one.
//  Make sure that anyone granted a lock actually asked for a lock.
//  Make sure that anyone who releases a lock has it.
//  Make sure that readers don't starve writers, and writers don't starve readers.  (Not sure how to code this up...)
{
    int reader_max=0;
    int writer_max=0;
    int state=0;
    char tstate[N];
    for (int i=0; i<N; i++) tstate[i]=0;
    for (int i=0; i<log_counter; i++) {
	switch (actionlog[i].action) {
	case 't': // fall through to 'T'
	case 'T':
	    assert(tstate[actionlog[i].threadid]==0);
	    tstate[actionlog[i].threadid]=actionlog[i].action;
	    break;
	case 'W':
	    assert(tstate[actionlog[i].threadid]=='T');
	    tstate[actionlog[i].threadid]=actionlog[i].action;
	    assert(state==0);
	    state=-1;
	    writer_max = 1;
	    break;
	case 'U':
	    assert(tstate[actionlog[i].threadid]=='W');
	    tstate[actionlog[i].threadid]=0;
	    assert(state==-1);
	    state=0;
	    break;
	case 'R':
	    assert(tstate[actionlog[i].threadid]=='t');
	    tstate[actionlog[i].threadid]=actionlog[i].action;
	    if (state<0) { printf("On step %d\n", i); }
	    assert(state>=0);
	    state++;
	    if (state>reader_max) reader_max=state;
	    break;
	case 'u':
	    assert(tstate[actionlog[i].threadid]=='R');
	    tstate[actionlog[i].threadid]=0;
	    assert(state>=0);
	    state--;
	    break;
	default:
	    abort();
	}
    }
    assert(reader_max>=expected_reader_parallelism_min);
    assert(reader_max<=expected_reader_parallelism_max);
    assert(writer_max==expected_writer_max_count);
}


static void test_rwlock_internal (void *(*start_th)(void*), int max_wr, int min_rd, int max_rd) {
    if (verbose>=2) printf("Running threads:\n");
    log_counter=0;
    pthread_t threads[N];
    int v[N];
    toku_fair_rwlock_init(&rwlock);
    for (int i=0; i<N; i++) {
	v[i]=i;
	int r = pthread_create(&threads[i], NULL, start_th, &v[i]);
	assert(r==0);
    }
    for (int i=0; i<N; i++) {
	void *rv;
	int r = pthread_join(threads[i], &rv);
	assert(rv==NULL);
	assert(r==0);
    }
    if (verbose>1) {
	for (int i=0; i<log_counter; i++) {
	    printf("%d: %*s%c%d\n", i, actionlog[i].threadid*4, "", actionlog[i].action, actionlog[i].loopid);
	}
    }
    check_actionlog(max_wr, min_rd, max_rd);
    toku_fair_rwlock_destroy(&rwlock);
    if (verbose>2) printf("OK\n");
}

static void test_rwlock (void) {
    test_rwlock_internal(start_thread, 1, 2, 3);
    for (int i=0; i<10; i++) {
	test_rwlock_internal(start_thread_random, 1, 0, N);
    }
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    if (timing_only) {
        if (1) { // to make it easy to only time the templated frwlock
            time_nop();
            time_fcall();
            time_cas();
            time_pthread_mutex();
            time_pthread_rwlock();
            time_newbrt_rwlock();
            time_newbrt_prelocked_rwlock();
            time_toku_cv_fair_rwlock();
            time_toku_fair_rwlock();
        }
	time_frwlock();
	time_frwlock_prelocked();
	if (verbose>0) {
            if (1) { // to make it easy to only time the templated frwlock
                printf("//  Best nop              time=%10.6fns\n", best_nop_time);
                printf("//  Best fcall            time=%10.6fns\n", best_fcall_time);
                printf("//  Best cas              time=%10.6fns\n", best_cas_time);
                printf("//  Best mutex            time=%10.6fns\n", best_mutex_time);
                printf("//  Best rwlock           time=%10.6fns\n", best_rwlock_time);
                printf("//  Best newbrt rwlock    time=%10.6fns\n", best_newbrt_time);
                printf("//  Best prelocked        time=%10.6fns\n", best_prelocked_time);
                printf("//  Best fair cv rwlock   time=%10.6fns\n", best_cv_fair_rwlock_time);
                printf("//  Best fair fast rwlock time=%10.6fns\n", best_fair_rwlock_time);
            }
            printf("//  Best frwlock         time=%10.6fns\n", best_frwlock_time);
            printf("//  Best frwlock_pre     time=%10.6fns\n", best_frwlock_prelocked_time);
	}
    } else {
	test_rwlock();
    }
    return 0;
}

