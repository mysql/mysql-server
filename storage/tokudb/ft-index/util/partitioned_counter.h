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

#ifndef UTIL_PARTITIONED_COUNTER_H
#define UTIL_PARTITIONED_COUNTER_H

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
// See partitioned_counter.cc for the abstraction function and representation invariant.
//
// The google style guide says to avoid using constructors, and it appears that
// constructors may have broken all the tests, because they called
// pthread_key_create before the key was actually created.  So the google style
// guide may have some wisdom there...
//
// This version does not use constructors, essentially reverrting to the google C++ style guide.
//

// The old C interface.  This required a bunch of explicit ___attribute__((__destructor__)) functions to remember to destroy counters at the end.
#if defined(__cplusplus)
extern "C" {
#endif

typedef struct partitioned_counter *PARTITIONED_COUNTER;
PARTITIONED_COUNTER create_partitioned_counter(void);
// Effect: Create a counter, initialized to zero.

void destroy_partitioned_counter(PARTITIONED_COUNTER);
// Effect: Destroy the counter.  No operations on that counter are permitted after this.

void increment_partitioned_counter(PARTITIONED_COUNTER, uint64_t amount);
// Effect: Increment the counter by amount.
// Requires: No overflows.  This is a 64-bit unsigned counter.

uint64_t read_partitioned_counter(PARTITIONED_COUNTER) __attribute__((__visibility__("default")));
// Effect: Return the current value of the counter.

void partitioned_counters_init(void);
// Effect: Initialize any partitioned counters data structures that must be set up before any partitioned counters run.

void partitioned_counters_destroy(void);
// Effect: Destroy any partitioned counters data structures.

#if defined(__cplusplus)
};
#endif

#if 0
#include <pthread.h>
#include "fttypes.h"

// Used inside the PARTITIONED_COUNTER.
struct linked_list_head {
    struct linked_list_element *first;
};


class PARTITIONED_COUNTER {
public:
    PARTITIONED_COUNTER(void);
    // Effect: Construct a counter, initialized to zero.

    ~PARTITIONED_COUNTER(void);
    // Effect: Destruct the counter.

    void increment(uint64_t amount);
    // Effect: Increment the counter by amount.  This is a 64-bit unsigned counter, and if you overflow it, you will get overflowed results (that is mod 2^64).
    // Requires: Don't use this from a static constructor or destructor.

    uint64_t read(void);
    // Effect: Read the sum.
    // Requires: Don't use this from a static constructor or destructor.

private:
    uint64_t       _sum_of_dead;             // The sum of all thread-local counts from threads that have terminated.
    pthread_key_t   _key;                     // The pthread_key which gives us the hook to construct and destruct thread-local storage.
    struct linked_list_head _ll_counter_head; // A linked list of all the thread-local information for this counter.
    
    // This function is used to destroy the thread-local part of the state when a thread terminates.
    // But it's not the destructor for the local part of the counter, it's a destructor on a "dummy" key just so that we get a notification when a thread ends.
    friend void destroy_thread_local_part_of_partitioned_counters (void *);
};
#endif

#endif // UTIL_PARTITIONED_COUNTER_H
