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

/* This code can either test the PARTITIONED_COUNTER abstraction or it can time various implementations. */

/* Try to make counter that requires no cache misses to increment, and to get the value can be slow.
 * I don't care much about races between the readers and writers on the counter.
 *
 * The problem: We observed that incrementing a counter with multiple threads is quite expensive.
 * Here are some performance numbers:
 * Machines:  mork or mindy (Intel Xeon L5520 2.27GHz)
 *            bradley's 4-core laptop laptop (Intel Core i7-2640M 2.80GHz) sandy bridge
 *            alf       16-core server (xeon E5-2665 2.4GHz) sandybridge
 *
 *      mork  mindy  bradley  alf
 *     1.22ns  1.07ns  1.27ns  0.61ns   to do a ++, but it's got a race in it.
 *    27.11ns 20.47ns 18.75ns 34.15ns   to do a sync_fetch_and_add().
 *     0.26ns  0.29ns  0.71ns  0.19ns   to do with a single version of a counter
 *     0.35ns  0.33ns  0.69ns  0.18ns   pure thread-local variable (no way to add things up)
 *             0.76ns  1.50ns  0.35ns   partitioned_counter.c (using link-time optimization, otherwise the function all overwhelms everything)
 *     2.21ns          3.32ns  0.70ns   partitioned_counter.c (using gcc, the C version at r46097, not C++)  This one is a little slower because it has an extra branch in it.
 * 
 * Surprisingly, compiling this code without -fPIC doesn't make it any faster (even the pure thread-local variable is the same).  -fPIC access to
 * thread-local variables look slower since they have a function all, but they don't seem to be any slower in practice.  In fact, even the puretl-ptr test
 * which simply increments a thread-local pointer is basically the same speed as accessing thread_local variable.
 * 
 * How it works.  Each thread has a thread-local counter structure with an integer in it.  To increment, we increment the thread-local structure.
 *   The other operation is to query the counters to get the sum of all the thread-local variables.
 *   The first time a pthread increments the variable we add the variable to a linked list.
 *   When a pthread ends, we use the pthread_key destructor to remove the variable from the linked list.  We also have to remember the sum of everything.
 *    that has been removed from the list.
 *   To get the sum we add the sum of the destructed items, plus everything in the list.
 *
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <toku_race_tools.h>
#include <toku_assert.h>
#include <portability/toku_atomic.h>
#include <memory.h>
#include <util/partitioned_counter.h>
#include "test.h"

// The test code includes the fastest version I could figure out to make, implemented below.

struct counter_s {
    bool inited;
    volatile int counter;
    struct counter_s *prev, *next;
    int myid;
};
static __thread struct counter_s counter = {false,0, NULL,NULL,0};

static int finished_counter=0; // counter for all threads that are done.

// We use a single mutex for anything complex.  We'd like to use a mutex per partitioned counter, but we must cope with the possibility of a race between
// a terminating pthread (which calls destroy_counter()), and a call to the counter destructor.  So we use a global mutex.
static pthread_mutex_t pc_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct counter_s *head=NULL;
static pthread_key_t   counter_key;

static void pc_lock (void)
// Effect: Lock the pc mutex.  
{
    int r = pthread_mutex_lock(&pc_mutex);
    assert(r==0);
}

static void pc_unlock (void)
// Effect: Unlock the pc mutex.
{
    int r = pthread_mutex_unlock(&pc_mutex);
    assert(r==0);
}

static void destroy_counter (void *counterp)
// Effect: This is the function passed to pthread_key_create that is to run whenever a thread terminates.
//   The thread-local part of the counter must be copied into the shared state, and the thread-local part of the counter must be
//   removed from the linked list of all thread-local parts.
{
    assert((struct counter_s*)counterp==&counter);
    pc_lock();
    if (counter.prev==NULL) {
	assert(head==&counter);
	head = counter.next;
    } else {
	counter.prev->next = counter.next;
    }
    if (counter.next!=NULL) {
	counter.next->prev = counter.prev;
    }
    finished_counter += counter.counter;
    TOKU_VALGRIND_HG_ENABLE_CHECKING(&counter.counter, sizeof(counter.counter)); // stop ignoring races
    //printf("finished counter now %d\n", finished_counter);
    pc_unlock();
}

static int idcounter=0;

static inline void increment (void) {
    if (!counter.inited) {
        pc_lock();
        struct counter_s *cp = &counter;
	{ int r = pthread_setspecific(counter_key, cp); assert(r==0); }
	cp->prev = NULL;
	cp->next = head;
	if (head!=NULL) {
	    head->prev = cp;
	}
        head = cp;
#ifdef __INTEL_COMPILER
        __memory_barrier(); // for some reason I don't understand, ICC needs a memory barrier here. -Bradley
#endif
	cp->counter = 0;
	cp->inited = true;
	cp->myid = idcounter++;
	TOKU_VALGRIND_HG_DISABLE_CHECKING(&counter.counter, sizeof(counter.counter)); // the counter increment is kind of racy.
        pc_unlock();
    }
    counter.counter++;
}

static int getvals (void) {
    pc_lock();
    int sum=finished_counter;
    for (struct counter_s *p=head; p; p=p->next) {
	sum+=p->counter;
    }
    pc_unlock();
    return sum;
}
    
/**********************************************************************************/
/* And now for some actual test code.                                             */
/**********************************************************************************/

static const int N=10000000;
static const int T=20;


PARTITIONED_COUNTER pc;
static void *pc_doit (void *v) {
    for (int i=0; i<N; i++) {
	increment_partitioned_counter(pc, 1);
    }
    //printf("val=%ld\n", read_partitioned_counter(pc));
    return v;
}

static void* new_doit (void* v) {
    for (int i=0; i<N; i++) {
	increment();
	//if (i%0x2000 == 0) sched_yield();
    }
    if (0) printf("done id=%d, getvals=%d\n", counter.myid, getvals());
    return v;
}

static int oldcounter=0;

static void* old_doit (void* v) {
    for (int i=0; i<N; i++) {
	(void)toku_sync_fetch_and_add(&oldcounter, 1);
	//if (i%0x1000 == 0) sched_yield();
    }
    return v;
}

static volatile int oldcounter_nonatomic=0;

static void* old_doit_nonatomic (void* v) {
    for (int i=0; i<N; i++) {
	oldcounter_nonatomic++;
	//if (i%0x1000 == 0) sched_yield();
    }
    return v;
}

static __thread volatile int thread_local_counter=0;
static void* tl_doit (void *v) {
    for (int i=0; i<N; i++) {
	thread_local_counter++;
    }
    return v;
}

static float tdiff (struct timeval *start, struct timeval *end) {
    return (end->tv_sec-start->tv_sec) +1e-6*(end->tv_usec - start->tv_usec);
}

static void pt_create (pthread_t *thread, void *(*f)(void*), void *extra) {
    int r = pthread_create(thread, NULL, f, extra);
    assert(r==0);
}

static void pt_join (pthread_t thread, void *expect_extra) {
    void *result;
    int r = pthread_join(thread, &result);
    assert(r==0);
    assert(result==expect_extra);
}

static void timeit (const char *description, void* (*f)(void*)) {
    struct timeval start, end;
    pthread_t threads[T];
    gettimeofday(&start, 0);
    for (int i=0; i<T; i++) {
	pt_create(&threads[i], f, NULL);
    }
    for (int i=0; i<T; i++) {
	pt_join(threads[i], NULL);
    }
    gettimeofday(&end, 0);
    printf("%-10s Time=%.6fs (%7.3fns per increment)\n", description, tdiff(&start, &end), (1e9*tdiff(&start, &end)/T)/N);
}

// Do a measurement where it really is only a pointer dereference to increment the variable, which is thread local.
static void* tl_doit_ptr (void *v) {
    volatile uint64_t *p = (uint64_t *)v;
    for (int i=0; i<N; i++) {
	(*p)++;
    }
    return v;
}


static void timeit_with_thread_local_pointer (const char *description, void* (*f)(void*)) {
    struct timeval start, end;
    pthread_t threads[T];
    struct { uint64_t values[8] __attribute__((__aligned__(64))); } values[T]; // pad to different cache lines.
    gettimeofday(&start, 0);
    for (int i=0; i<T; i++) {
        values[i].values[0]=0;
	pt_create(&threads[i], f, &values[i].values[0]);
    }
    for (int i=0; i<T; i++) {
	pt_join(threads[i], &values[i].values[0]);
    }
    gettimeofday(&end, 0);
    printf("%-10s Time=%.6fs (%7.3fns per increment)\n", description, tdiff(&start, &end), (1e9*tdiff(&start, &end)/T)/N);
}

static int verboseness_cmdarg=0;
static bool time_cmdarg=false;

static void parse_args (int argc, const char *argv[]) {
    const char *progname = argv[1];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v")==0) verboseness_cmdarg++;
	else if (strcmp(argv[0], "--time")==0) time_cmdarg=true;
	else {
	    printf("Usage: %s [-v] [--time]\n Default is to run tests.  --time produces timing output.\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
}

static void do_timeit (void) {
    { int r = pthread_key_create(&counter_key, destroy_counter); assert(r==0); } 
    printf("%d threads\n%d increments per thread\n", T, N);
    timeit("++",         old_doit_nonatomic);
    timeit("atomic++",   old_doit);
    timeit("fast",       new_doit);
    timeit("puretl",     tl_doit);
    timeit_with_thread_local_pointer("puretl-ptr", tl_doit_ptr);
    pc = create_partitioned_counter();
    timeit("pc",       pc_doit);
    destroy_partitioned_counter(pc);
}

struct test_arguments {
    PARTITIONED_COUNTER pc;
    uint64_t            limit;
    uint64_t            total_increment_per_writer;
    volatile uint64_t   unfinished_count;
};

static void *reader_test_fun (void *ta_v) {
    struct test_arguments *ta = (struct test_arguments *)ta_v;
    uint64_t lastval = 0;
    while (ta->unfinished_count>0) {
	uint64_t thisval = read_partitioned_counter(ta->pc);
	assert(lastval <= thisval);
	assert(thisval <= ta->limit+2);
	lastval = thisval;
	if (verboseness_cmdarg && (0==(thisval & (thisval-1)))) printf("ufc=%" PRIu64 " Thisval=%" PRIu64 "\n", ta->unfinished_count,thisval);
    }
    uint64_t thisval = read_partitioned_counter(ta->pc);
    assert(thisval==ta->limit+2); // we incremented two extra times in the test
    return ta_v;
}

static void *writer_test_fun (void *ta_v) {
    struct test_arguments *ta = (struct test_arguments *)ta_v;
    for (uint64_t i=0; i<ta->total_increment_per_writer; i++) {
	if (i%1000 == 0) sched_yield();
	increment_partitioned_counter(ta->pc, 1);
    }
    uint64_t c __attribute__((__unused__)) = toku_sync_fetch_and_sub(&ta->unfinished_count, 1);
    return ta_v;
}
    

static void do_testit (void) {
    const int NGROUPS = 2;
    uint64_t limits[NGROUPS];
    limits [0] = 2000000;
    limits [1] = 1000000;
    uint64_t n_writers[NGROUPS];
    n_writers[0] = 20;
    n_writers[1] = 40;
    struct test_arguments tas[NGROUPS];
    pthread_t reader_threads[NGROUPS];
    pthread_t *writer_threads[NGROUPS];
    for (int i=0; i<NGROUPS; i++) {
        tas[i].pc                         = create_partitioned_counter();
	tas[i].limit                      = limits[i];
	tas[i].unfinished_count           = n_writers[i];
	tas[i].total_increment_per_writer = limits[i]/n_writers[i];
	assert(tas[i].total_increment_per_writer * n_writers[i] == limits[i]);
	pt_create(&reader_threads[i], reader_test_fun, &tas[i]);
        increment_partitioned_counter(tas[i].pc, 1); // make sure that the long-lived thread also increments the partitioned counter, to test for #5321.
	MALLOC_N(n_writers[i], writer_threads[i]);
	for (uint64_t j=0; j<n_writers[i] ; j++) {
	    pt_create(&writer_threads[i][j], writer_test_fun, &tas[i]);
	}
        increment_partitioned_counter(tas[i].pc, 1); // make sure that the long-lived thread also increments the partitioned counter, to test for #5321.
    }
    for (int i=0; i<NGROUPS; i++) {
	pt_join(reader_threads[i], &tas[i]);
	for (uint64_t j=0; j<n_writers[i] ; j++) {
	    pt_join(writer_threads[i][j], &tas[i]);
	}
	toku_free(writer_threads[i]);
        destroy_partitioned_counter(tas[i].pc);
    }
}

volatile int spinwait=0;
static void* test2_fun (void* mypc_v) {
    PARTITIONED_COUNTER mypc = (PARTITIONED_COUNTER)mypc_v;
    increment_partitioned_counter(mypc, 3);
    spinwait=1;
    while (spinwait==1);
    // mypc no longer points at a valid data structure.
    return NULL;
}

static void do_testit2 (void) 
// This test checks to see what happens if a thread is still live when we destruct a counter.
//   A thread increments the counter, then lets us know through a spin wait, then waits until we destroy the counter.
{
    pthread_t t;
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&spinwait, sizeof(spinwait)); // this is a racy volatile variable.
    {
        PARTITIONED_COUNTER mypc = create_partitioned_counter();
        increment_partitioned_counter(mypc, 1); // make sure that the long-lived thread also increments the partitioned counter, to test for #5321.
        pt_create(&t, test2_fun, mypc);
        while(spinwait==0); // wait until he incremented the counter.
        increment_partitioned_counter(mypc, -1);
        assert(read_partitioned_counter(mypc)==3);
        destroy_partitioned_counter(mypc);
    } // leave scope, so the counter goes away.
    spinwait=2; // tell the other guy to finish up.
    pt_join(t, NULL);
}

int test_main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    if (time_cmdarg) {
	do_timeit();
    } else {
	do_testit();
        do_testit2();
    }
    return 0;
}
