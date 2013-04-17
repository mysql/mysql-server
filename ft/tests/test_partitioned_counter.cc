/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
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
 *      0.3ns  1.07ns  1.27ns  0.58ns   to do a ++, but it's got a race in it.
 *     28.0ns 20.47ns 18.75ns 39.38ns   to do a sync_fetch_and_add().
 *      0.4ns  0.29ns  0.71ns  0.19ns   to do with a single version of a counter
 *             0.33ns  0.69ns  0.18ns   pure thread-local variable (no way to add things up)
 *             0.76ns  1.50ns  0.35ns   partitioned_counter.c (using link-time optimization, otherwise the function all overwhelms everything)
 *      
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
#include <valgrind/helgrind.h>
#include "toku_assert.h"
#include "partitioned_counter.h"
#include "memory.h"

struct counter_s {
    bool inited;
    int counter;
    struct counter_s *prev, *next;
    int myid;
};
static __thread struct counter_s counter = {false,0, NULL,NULL,0};

static int finished_counter=0; // counter for all threads that are done.
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static struct counter_s *head=NULL, *tail=NULL;
static pthread_key_t   counter_key;

static void destroy_counter (void *counterp) {
    assert((struct counter_s*)counterp==&counter);
    { int r = pthread_mutex_lock(&mutex); assert(r==0); }
    if (counter.prev==NULL) {
	assert(head==&counter);
	head = counter.next;
    } else {
	counter.prev->next = counter.next;
    }
    if (counter.next==NULL) {
	assert(tail==&counter);
	tail = counter.prev;
    } else {
	counter.next->prev = counter.prev;
    }
    finished_counter += counter.counter;
    HELGRIND_VALGRIND_HG_ENABLE_CHECKING(&counter.counter, sizeof(counter.counter)); // stop ignoring races
    //printf("finished counter now %d\n", finished_counter);
    { int r = pthread_mutex_unlock(&mutex); assert(r==0); }
}

static int idcounter=0;

static inline void increment (void) {
    if (!counter.inited) {
	{ int r = pthread_mutex_lock(&mutex); assert(r==0); }
	{ int r = pthread_setspecific(counter_key, &counter); assert(r==0); }
	counter.prev = tail;
	counter.next = NULL;
	if (head==NULL) {
	    head = &counter;
	    tail = &counter;
	} else {
	    tail->next = &counter;
	    tail = &counter;
	}
	counter.counter = 0;
	counter.inited = true;
	counter.myid = idcounter++;
	HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&counter.counter, sizeof(counter.counter)); // the counter increment is kind of racy.
	{ int r = pthread_mutex_unlock(&mutex); assert(r==0); }
    }
    counter.counter++;
}

static int getvals (void) {
    { int r = pthread_mutex_lock(&mutex); assert(r==0); }    
    int sum=finished_counter;
    for (struct counter_s *p=head; p; p=p->next) {
	sum+=p->counter;
    }
    { int r = pthread_mutex_unlock(&mutex); assert(r==0); }
    return sum;
}
    

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
	(void)__sync_fetch_and_add(&oldcounter, 1);
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
    printf("%-9s Time=%.6fs (%7.3fns per increment)\n", description, tdiff(&start, &end), (1e9*tdiff(&start, &end)/T)/N);
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
    pc = create_partitioned_counter();
    printf("%d threads\n%d increments per thread\n", T, N);
    timeit("++",       old_doit_nonatomic);
    timeit("atomic++", old_doit);
    timeit("fast",     new_doit);
    timeit("puretl",   tl_doit);
    timeit("pc",       pc_doit);
    destroy_partitioned_counter(pc);
}

struct test_arguments {
    PARTITIONED_COUNTER pc;
    unsigned long limit;
    unsigned long total_increment_per_writer;
    volatile unsigned long unfinished_count;
};

static void *reader_test_fun (void *ta_v) {
    struct test_arguments *ta = (struct test_arguments *)ta_v;
    unsigned long lastval = 0;
    printf("reader starting\n");
    while (ta->unfinished_count>0) {
	unsigned long thisval = read_partitioned_counter(ta->pc);
	assert(lastval <= thisval);
	assert(thisval <= ta->limit);
	lastval = thisval;
	if (verboseness_cmdarg && (0==(thisval & (thisval-1)))) printf("Thisval=%ld\n", thisval);
    }
    unsigned long thisval = read_partitioned_counter(ta->pc);
    assert(thisval==ta->limit);
    return ta_v;
}

static void *writer_test_fun (void *ta_v) {
    struct test_arguments *ta = (struct test_arguments *)ta_v;
    printf("writer starting\n");
    for (unsigned long i=0; i<ta->total_increment_per_writer; i++) {
	if (i%1000 == 0) sched_yield();
	increment_partitioned_counter(ta->pc, 1);
    }
    printf("writer done\n");
    __sync_fetch_and_sub(&ta->unfinished_count, 1);
    return ta_v;
}
    

static void do_testit (void) {
    const int NGROUPS = 2;
    PARTITIONED_COUNTER pcs[NGROUPS];
    unsigned long limits[NGROUPS];
    limits [0] = 2000000;
    limits [1] = 1000000;
    unsigned long n_writers[NGROUPS];
    n_writers[0] = 20;
    n_writers[1] = 40;
    struct test_arguments tas[NGROUPS];
    pthread_t reader_threads[NGROUPS];
    pthread_t *writer_threads[NGROUPS];
    for (int i=0; i<NGROUPS; i++) {
	pcs[i] = create_partitioned_counter();
	tas[i].pc = pcs[i];
	tas[i].limit = limits[i];
	tas[i].unfinished_count  = n_writers[i];
	tas[i].total_increment_per_writer = limits[i]/n_writers[i];
	assert(tas[i].total_increment_per_writer * n_writers[i] == limits[i]);
	pt_create(&reader_threads[i], reader_test_fun, &tas[i]);
	MALLOC_N(n_writers[i], writer_threads[i]);
	for (unsigned long j=0; j<n_writers[i] ; j++) {
	    pt_create(&writer_threads[i][j], writer_test_fun, &tas[i]);
	}
    }
    for (int i=0; i<NGROUPS; i++) {
	pt_join(reader_threads[i], &tas[i]);
	for (unsigned long j=0; j<n_writers[i] ; j++) {
	    pt_join(writer_threads[i][j], &tas[i]);
	}
	toku_free(writer_threads[i]);
	destroy_partitioned_counter(pcs[i]);
    }
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    if (time_cmdarg) {
	do_timeit();
    } else {
	do_testit();
    }
    return 0;
}
