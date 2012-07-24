/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef THREAD_LOCAL_COUNTER_H
#define THREAD_LOCAL_COUNTER_H

// Overview: A partitioned_counter provides a counter that can be incremented and the running sum can be read at any time.
//  We assume that increments are frequent, whereas reading is infrequent.
// Implementation hint: Use thread-local storage so each thread increments its own data.  The increment does not require a lock or atomic operation.
//  Reading the data can be performed by iterating over the thread-local versions, summing them up.
//  The data structure also includes a sum for all the threads that have died.
//  Use a pthread_key to create the thread-local versions.  When a thread finishes, the system calls pthread_key destructor which can add that thread's copy
//  into the sum_of_dead counter.
// Rationale: For statistics such as are found in engine status, we need a counter that requires no cache misses to increment.  We've seen significant
//  performance speedups by removing certain counters.  Rather than removing those statistics, we would like to just make the counter fast.
//  We generally increment the counters frequently, and want to fetch the values infrequently.
//  The counters are monotonic.
//  The counters can be split into many counters, which can be summed up at the end.
//  We don't care if we get slightly out-of-date counter sums when we read the counter.  We don't care if there is a race on reading the a counter
//   variable and incrementing.
//  See tests/test_partitioned_counter.c for some performance measurements.
// Operations:
//   create_partitioned_counter    Create a counter initialized to zero.
//   destroy_partitioned_counter   Destroy it.
//   increment_partitioned_counter Increment it.  This is the frequent operation.
//   read_partitioned_counter      Get the current value.  This is infrequent.

typedef struct partitioned_counter *PARTITIONED_COUNTER;
PARTITIONED_COUNTER create_partitioned_counter(void);
// Effect: Create a counter, initialized to zero.

void destroy_partitioned_counter (PARTITIONED_COUNTER);
// Effect: Destroy the counter.  No operations on that counter are permitted after this.

void increment_partitioned_counter (PARTITIONED_COUNTER, unsigned long amount);
// Effect: Increment the counter by amount.
// Requires: No overflows.  This is a 64-bit unsigned counter.

unsigned long read_partitioned_counter (PARTITIONED_COUNTER);
// Effect: Return the current value of the counter.

#endif
