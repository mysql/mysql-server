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

#include <memory.h>
#include <toku_portability.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <toku_assert.h>
#include <toku_list.h>
#include <portability/toku_pthread.h>

#include "threadpool.h"

struct toku_thread {
    struct toku_thread_pool *pool;
    toku_pthread_t tid;
    void *(*f)(void *arg);
    void *arg;
    int doexit;
    struct toku_list free_link;
    struct toku_list all_link;
    toku_cond_t wait;
};

struct toku_thread_pool {
    int max_threads;
    int cur_threads;
    struct toku_list free_threads;
    struct toku_list all_threads;

    toku_mutex_t lock;
    toku_cond_t wait_free;
    
    uint64_t gets, get_blocks;
};

static void *toku_thread_run_internal(void *arg);
static void toku_thread_pool_lock(struct toku_thread_pool *pool);
static void toku_thread_pool_unlock(struct toku_thread_pool *pool);

static int 
toku_thread_create(struct toku_thread_pool *pool, struct toku_thread **toku_thread_return) {
    int r;
    struct toku_thread *MALLOC(thread);
    if (thread == NULL) {
        r = get_error_errno();
    } else {
        memset(thread, 0, sizeof *thread);
        thread->pool = pool;
        toku_cond_init(&thread->wait, NULL);
        r = toku_pthread_create(&thread->tid, NULL, toku_thread_run_internal, thread); resource_assert_zero(r);
        *toku_thread_return = thread;
    }
    return r;
}

void 
toku_thread_run(struct toku_thread *thread, void *(*f)(void *arg), void *arg) {
    toku_thread_pool_lock(thread->pool);
    thread->f = f;
    thread->arg = arg;
    toku_cond_signal(&thread->wait);
    toku_thread_pool_unlock(thread->pool);
}

static void 
toku_thread_destroy(struct toku_thread *thread) {
    int r;
    void *ret;
    r = toku_pthread_join(thread->tid, &ret); invariant(r == 0 && ret == thread);
    struct toku_thread_pool *pool = thread->pool;
    toku_thread_pool_lock(pool);
    toku_list_remove(&thread->free_link);
    toku_thread_pool_unlock(pool);
    toku_cond_destroy(&thread->wait);
    toku_free(thread);
}

static void 
toku_thread_ask_exit(struct toku_thread *thread) {
    thread->doexit = 1;
    toku_cond_signal(&thread->wait);
}

static void *
toku_thread_run_internal(void *arg) {
    struct toku_thread *thread = (struct toku_thread *) arg;
    struct toku_thread_pool *pool = thread->pool;
    toku_thread_pool_lock(pool);
    while (1) {
        toku_cond_signal(&pool->wait_free);
        void *(*thread_f)(void *); void *thread_arg; int doexit;
        while (1) {
            thread_f = thread->f; thread_arg = thread->arg; doexit = thread->doexit; // make copies of these variables to make helgrind happy
            if (thread_f || doexit) 
                break;
            toku_cond_wait(&thread->wait, &pool->lock);
        }
        toku_thread_pool_unlock(pool);
        if (thread_f)
            (void) thread_f(thread_arg);
        if (doexit)
            break;
        toku_thread_pool_lock(pool);
        thread->f = NULL;
        toku_list_push(&pool->free_threads, &thread->free_link);
    }
    return arg;
}      

int 
toku_thread_pool_create(struct toku_thread_pool **pool_return, int max_threads) {
    int r;
    struct toku_thread_pool *CALLOC(pool);
    if (pool == NULL) {
        r = get_error_errno();
    } else {
        toku_mutex_init(&pool->lock, NULL);
        toku_list_init(&pool->free_threads);
        toku_list_init(&pool->all_threads);
        toku_cond_init(&pool->wait_free, NULL);
        pool->cur_threads = 0;
        pool->max_threads = max_threads;
        *pool_return = pool;
        r = 0;
    }
    return r;
}    

static void 
toku_thread_pool_lock(struct toku_thread_pool *pool) {
    toku_mutex_lock(&pool->lock);
}

static void 
toku_thread_pool_unlock(struct toku_thread_pool *pool) {
    toku_mutex_unlock(&pool->lock);
}

void 
toku_thread_pool_destroy(struct toku_thread_pool **poolptr) {
    struct toku_thread_pool *pool = *poolptr;
    *poolptr = NULL;

    // ask the threads to exit
    toku_thread_pool_lock(pool);
    struct toku_list *list;
    for (list = pool->all_threads.next; list != &pool->all_threads; list = list->next) {
        struct toku_thread *thread = toku_list_struct(list, struct toku_thread, all_link);
        toku_thread_ask_exit(thread);
    }
    toku_thread_pool_unlock(pool);

    // wait for all of the threads to exit
    while (!toku_list_empty(&pool->all_threads)) {
        list = toku_list_pop_head(&pool->all_threads);
        struct toku_thread *thread = toku_list_struct(list, struct toku_thread, all_link);
        toku_thread_destroy(thread);
        pool->cur_threads -= 1;
    }

    invariant(pool->cur_threads == 0);
    
    // cleanup
    toku_cond_destroy(&pool->wait_free);
    toku_mutex_destroy(&pool->lock);
    
    toku_free(pool);
}

static int 
toku_thread_pool_add(struct toku_thread_pool *pool) {
    struct toku_thread *thread = NULL;
    int r = toku_thread_create(pool, &thread); 
    if (r == 0) {
        pool->cur_threads += 1;
        toku_list_push(&pool->all_threads, &thread->all_link);
        toku_list_push(&pool->free_threads, &thread->free_link);
        toku_cond_signal(&pool->wait_free);
    }
    return r;
}   

// get one thread from the free pool.  
static int 
toku_thread_pool_get_one(struct toku_thread_pool *pool, int dowait, struct toku_thread **toku_thread_return) {
    int r = 0;
    toku_thread_pool_lock(pool);
    pool->gets++;
    while (1) {
        if (!toku_list_empty(&pool->free_threads))
            break;
        if (pool->max_threads == 0 || pool->cur_threads < pool->max_threads)
            (void) toku_thread_pool_add(pool);
        if (toku_list_empty(&pool->free_threads) && !dowait) {
            r = EWOULDBLOCK;
            break;
        }
        pool->get_blocks++;
        toku_cond_wait(&pool->wait_free, &pool->lock);
    }
    if (r == 0) {
        struct toku_list *list = toku_list_pop_head(&pool->free_threads);
        struct toku_thread *thread = toku_list_struct(list, struct toku_thread, free_link);
        *toku_thread_return = thread;
    } else
        *toku_thread_return = NULL;
    toku_thread_pool_unlock(pool);
    return r;
}

int 
toku_thread_pool_get(struct toku_thread_pool *pool, int dowait, int *nthreads, struct toku_thread **toku_thread_return) {
    int r = 0;
    int n = *nthreads;
    int i;
    for (i = 0; i < n; i++) {
        r = toku_thread_pool_get_one(pool, dowait, &toku_thread_return[i]);
        if (r != 0)
            break;
    }
    *nthreads = i;
    return r;
}

int 
toku_thread_pool_run(struct toku_thread_pool *pool, int dowait, int *nthreads, void *(*f)(void *arg), void *arg) {
    int n = *nthreads;
    struct toku_thread *tids[n];
    int r = toku_thread_pool_get(pool, dowait, nthreads, tids);
    if (r == 0 || r == EWOULDBLOCK) {
        n = *nthreads;
        for (int i = 0; i < n; i++)
            toku_thread_run(tids[i], f, arg);
    }
    return r;
}

void 
toku_thread_pool_print(struct toku_thread_pool *pool, FILE *out) {
    fprintf(out, "%s:%d %p %llu %llu\n", __FILE__, __LINE__, pool, (long long unsigned) pool->gets, (long long unsigned) pool->get_blocks);
}

int 
toku_thread_pool_get_current_threads(struct toku_thread_pool *pool) {
    return pool->cur_threads;
}
