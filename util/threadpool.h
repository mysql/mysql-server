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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#pragma once

#include <stdio.h>

// A toku_thread is toku_pthread that can be cached.
struct toku_thread;

// Run a function f on a thread
// This function setups up the thread to run function f with argument arg and then wakes up
// the thread to run it.
void toku_thread_run(struct toku_thread *thread, void *(*f)(void *arg), void *arg);

// A toku_thread_pool is a pool of toku_threads.  These threads can be allocated from the pool
// and can run an arbitrary function.
struct toku_thread_pool;

typedef struct toku_thread_pool *THREADPOOL;

// Create a new threadpool
// Effects: a new threadpool is allocated and initialized. the number of threads in the threadpool is limited to max_threads.  
// If max_threads == 0 then there is no limit on the number of threads in the pool.
// Initially, there are no threads in the pool. Threads are allocated by the _get or _run functions.
// Returns: if there are no errors, the threadpool is set and zero is returned.  Otherwise, an error number is returned.
int toku_thread_pool_create(struct toku_thread_pool **threadpoolptr, int max_threads);

// Destroy a threadpool
// Effects: the calling thread joins with all of the threads in the threadpool.
// Effects: the threadpool memory is freed.
// Returns: the threadpool is set to null.
void toku_thread_pool_destroy(struct toku_thread_pool **threadpoolptr);

// Get the current number of threads in the thread pool
int toku_thread_pool_get_current_threads(struct toku_thread_pool *pool);

// Get one or more threads from the thread pool
// dowait indicates whether or not the caller blocks waiting for threads to free up
// nthreads on input determines the number of threads that are wanted
// nthreads on output indicates the number of threads that were allocated
// toku_thread_return on input supplies an array of thread pointers (all NULL).  This function returns the threads
// that were allocated in the array.
int toku_thread_pool_get(struct toku_thread_pool *pool, int dowait, int *nthreads, struct toku_thread **toku_thread_return);

// Run a function f on one or more threads allocated from the thread pool
int toku_thread_pool_run(struct toku_thread_pool *pool, int dowait, int *nthreads, void *(*f)(void *arg), void *arg);

// Print the state of the thread pool
void toku_thread_pool_print(struct toku_thread_pool *pool, FILE *out);
