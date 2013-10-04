/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef UTIL_RWLOCK_H
#define UTIL_RWLOCK_H
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

#include <toku_assert.h>

/* Readers/writers locks implementation
 *
 *****************************************
 *     Overview
 *****************************************
 *
 * TokuDB employs readers/writers locks for the ephemeral locks (e.g.,
 * on FT nodes) Why not just use the toku_pthread_rwlock API?
 *
 *   1) we need multiprocess rwlocks (not just multithreaded)
 *
 *   2) pthread rwlocks are very slow since they entail a system call
 *   (about 2000ns on a 2GHz T2500.)
 *
 *     Related: We expect the common case to be that the lock is
 *     granted
 *
 *   3) We are willing to employ machine-specific instructions (such
 *   as atomic exchange, and mfence, each of which runs in about
 *   10ns.)
 *
 *   4) We want to guarantee nonstarvation (many rwlock
 *   implementations can starve the writers because another reader
 *   comes * along before all the other readers have unlocked.)
 *
 *****************************************
 *      How it works
 *****************************************
 *
 * We arrange that the rwlock object is in the address space of both
 * threads or processes.  For processes we use mmap().
 *
 * The rwlock struct comprises the following fields
 *
 *    a long mutex field (which is accessed using xchgl() or other
 *    machine-specific instructions.  This is a spin lock.
 *
 *    a read counter (how many readers currently have the lock?)
 *
 *    a write boolean (does a writer have the lock?)
 *
 *    a singly linked list of semaphores for waiting requesters.  This
 *    list is sorted oldest requester first.  Each list element
 *    contains a semaphore (which is provided by the requestor) and a
 *    boolean indicating whether it is a reader or a writer.
 *
 * To lock a read rwlock:
 *
 *    1) Acquire the mutex.
 *
 *    2) If the linked list is not empty or the writer boolean is true
 *    then
 *
 *       a) initialize your semaphore (to 0),
 *       b) add your list element to the end of the list (with  rw="read")
 *       c) release the mutex
 *       d) wait on the semaphore
 *       e) when the semaphore release, return success.
 *
 *    3) Otherwise increment the reader count, release the mutex, and
 *    return success.
 *
 * To lock the write rwlock is almost the same.
 *     1) Acquire the mutex
 *     2) If the list is not empty or the reader count is nonzero
 *        a) initialize semaphore
 *        b) add to end of list (with rw="write")
 *        c) release mutex
 *        d) wait on the semaphore
 *        e) return success when the semaphore releases
 *     3) Otherwise set writer=true, release mutex and return success.
 *
 * To unlock a read rwlock:
 *     1) Acquire mutex
 *     2) Decrement reader count
 *     3) If the count is still positive or the list is empty then
 *        return success
 *     4) Otherwise (count==zero and the list is nonempty):
 *        a) If the first element of the list is a reader:
 *            i) while the first element is a reader:
 *                 x) pop the list
 *                 y) increment the reader count
 *                 z) increment the semaphore (releasing it for some waiter)
 *            ii) return success
 *        b) Else if the first element is a writer
 *            i) pop the list
 *            ii) set writer to true
 *            iii) increment the semaphore
 *            iv) return success
 */

//Use case:
// A read lock is acquired by threads that get and pin an entry in the
// cachetable. A write lock is acquired by the writer thread when an entry
// is evicted from the cachetable and is being written storage.

//Use case:
// General purpose reader writer lock with properties:
// 1. multiple readers, no writers
// 2. one writer at a time
// 3. pending writers have priority over pending readers

// An external mutex must be locked when using these functions.  An alternate
// design would bury a mutex into the rwlock itself.  While this may
// increase parallelism at the expense of single thread performance, we
// are experimenting with a single higher level lock.

typedef struct rwlock *RWLOCK;
struct rwlock {
    int reader;                  // the number of readers
    int want_read;                // the number of blocked readers
    toku_cond_t wait_read;
    int writer;                  // the number of writers
    int want_write;              // the number of blocked writers
    toku_cond_t wait_write;
    toku_cond_t* wait_users_go_to_zero;
};

// returns: the sum of the number of readers, pending readers, writers, and
// pending writers

static inline int rwlock_users(RWLOCK rwlock) {
    return rwlock->reader + rwlock->want_read + rwlock->writer + rwlock->want_write;
}

// initialize a read write lock

static __attribute__((__unused__))
void
rwlock_init(RWLOCK rwlock) {
    rwlock->reader = rwlock->want_read = 0;
    toku_cond_init(&rwlock->wait_read, 0);
    rwlock->writer = rwlock->want_write = 0;
    toku_cond_init(&rwlock->wait_write, 0);
    rwlock->wait_users_go_to_zero = NULL;
}

// destroy a read write lock

static __attribute__((__unused__))
void
rwlock_destroy(RWLOCK rwlock) {
    paranoid_invariant(rwlock->reader == 0);
    paranoid_invariant(rwlock->want_read == 0);
    paranoid_invariant(rwlock->writer == 0);
    paranoid_invariant(rwlock->want_write == 0);
    toku_cond_destroy(&rwlock->wait_read);
    toku_cond_destroy(&rwlock->wait_write);
}

// obtain a read lock
// expects: mutex is locked

static inline void rwlock_read_lock(RWLOCK rwlock, toku_mutex_t *mutex) {
    paranoid_invariant(!rwlock->wait_users_go_to_zero);
    if (rwlock->writer || rwlock->want_write) {
        rwlock->want_read++;
        while (rwlock->writer || rwlock->want_write) {
            toku_cond_wait(&rwlock->wait_read, mutex);
        }
        rwlock->want_read--;
    }
    rwlock->reader++;
}

// release a read lock
// expects: mutex is locked

static inline void rwlock_read_unlock(RWLOCK rwlock) {
    paranoid_invariant(rwlock->reader > 0);
    paranoid_invariant(rwlock->writer == 0);
    rwlock->reader--;
    if (rwlock->reader == 0 && rwlock->want_write) {
        toku_cond_signal(&rwlock->wait_write);
    }
    if (rwlock->wait_users_go_to_zero && rwlock_users(rwlock) == 0) {
        toku_cond_signal(rwlock->wait_users_go_to_zero);
    }
}

// obtain a write lock
// expects: mutex is locked

static inline void rwlock_write_lock(RWLOCK rwlock, toku_mutex_t *mutex) {
    paranoid_invariant(!rwlock->wait_users_go_to_zero);
    if (rwlock->reader || rwlock->writer) {
        rwlock->want_write++;
        while (rwlock->reader || rwlock->writer) {
            toku_cond_wait(&rwlock->wait_write, mutex);
        }
        rwlock->want_write--;
    }
    rwlock->writer++;
}

// release a write lock
// expects: mutex is locked

static inline void rwlock_write_unlock(RWLOCK rwlock) {
    paranoid_invariant(rwlock->reader == 0);
    paranoid_invariant(rwlock->writer == 1);
    rwlock->writer--;
    if (rwlock->want_write) {
        toku_cond_signal(&rwlock->wait_write);
    } else if (rwlock->want_read) {
        toku_cond_broadcast(&rwlock->wait_read);
    }    
    if (rwlock->wait_users_go_to_zero && rwlock_users(rwlock) == 0) {
        toku_cond_signal(rwlock->wait_users_go_to_zero);
    }
}

// returns: the number of readers

static inline int rwlock_readers(RWLOCK rwlock) {
    return rwlock->reader;
}

// returns: the number of readers who are waiting for the lock

static inline int rwlock_blocked_readers(RWLOCK rwlock) {
    return rwlock->want_read;
}

// returns: the number of writers who are waiting for the lock

static inline int rwlock_blocked_writers(RWLOCK rwlock) {
    return rwlock->want_write;
}

// returns: the number of writers

static inline int rwlock_writers(RWLOCK rwlock) {
    return rwlock->writer;
}

static inline bool rwlock_write_will_block(RWLOCK rwlock) {
    return (rwlock->writer > 0 || rwlock->reader > 0);
}

static inline int rwlock_read_will_block(RWLOCK rwlock) {
    return (rwlock->writer > 0 || rwlock->want_write > 0);
}

static inline void rwlock_wait_for_users(
    RWLOCK rwlock,
    toku_mutex_t *mutex
    )
{
    paranoid_invariant(!rwlock->wait_users_go_to_zero);
    toku_cond_t cond;
    toku_cond_init(&cond, NULL);
    while (rwlock_users(rwlock) > 0) {
        rwlock->wait_users_go_to_zero = &cond;
        toku_cond_wait(&cond, mutex);
    }
    rwlock->wait_users_go_to_zero = NULL;
    toku_cond_destroy(&cond);
}

#endif // UTIL_RWLOCK_H
