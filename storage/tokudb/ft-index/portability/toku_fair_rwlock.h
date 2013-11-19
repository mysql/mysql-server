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

#include "toku_pthread.h"
#include <portability/toku_atomic.h>

// Fair readers/writer locks.  These are fair (meaning first-come first-served.  No reader starvation, and no writer starvation).  And they are
// probably faster than the linux readers/writer locks (pthread_rwlock_t).
struct toku_fair_rwlock_waiter_state; // this structure is used internally.
typedef struct toku_fair_rwlock_s {
    // Try to put enough state into STATE so that in many cases, a compare-and-swap will work.
    // The 64-bit state bits are as follows:
    //    bit 0:      1 iff someone has exclusive ownership of the entire state.  (This is a spin lock).
    //    bit 1:      1 iff someone holds a write lock.
    //    bit 2:      1 iff the queue is not empty (if the queue is not empty, then you cannot use the fast path.)
    //    bits 3--31  how many read locks are held.
    uint64_t volatile                     state;
    // The waiters each provide a condition variable.  This is the mutex they are all using.
    // If anyone holds this mutex, they must set the RWS_MUTEXED bit first.  Then they grab the mutex.  Then they clear the bit indicating that they don't have the mutex.  No one else can change anything while the RWS_MUTEXED bit is set.
    toku_mutex_t                  mutex;
    struct toku_fair_rwlock_waiter_state *waiters_head, *waiters_tail;
} toku_fair_rwlock_t;

void toku_rwlock_init(void) __attribute__((constructor));
void toku_rwlock_destroy(void) __attribute__((destructor));

#define RWS_WLOCK_MASK 1LL

#define RWS_QCOUNT_OFF  1
#define RWS_QCOUNT_LEN  31
#define RWS_QCOUNT_INCR 2LL
#define RWS_QCOUNT_MASK (((1L<<RWS_QCOUNT_LEN)-1)<<RWS_QCOUNT_OFF)

#define RWS_RCOUNT_OFF  (RWS_QCOUNT_OFF+RWS_QCOUNT_LEN)
#define RWS_RCOUNT_LEN  31
#define RWS_RCOUNT_INCR (1LL<<32)

static inline int s_get_wlock(uint64_t s) {
    return (s&RWS_WLOCK_MASK)!=0;
}
static inline unsigned int s_get_qcount(uint64_t s) {
    return (s>>RWS_QCOUNT_OFF)&((1LL<<RWS_QCOUNT_LEN)-1);
}
static inline unsigned int s_get_rcount(uint64_t s) {
    return (s>>RWS_RCOUNT_OFF)&((1LL<<RWS_RCOUNT_LEN)-1);
}

static inline uint64_t s_set_wlock (uint64_t s) {
    return s | RWS_WLOCK_MASK;
}
static inline uint64_t s_clear_wlock (uint64_t s) {
    return s & ~RWS_WLOCK_MASK;
}
static inline uint64_t s_incr_qcount (uint64_t s) {
    //printf("%s:%d (%s) s=%lx, get_qcount=%d 1u<<%d=%u\n", __FILE__, __LINE__, __FUNCTION__, s, s_get_qcount(s), RWS_QCOUNT_LEN, 1u<<RWS_QCOUNT_LEN);
    //assert(s_get_qcount(s)+1 < (1u<<RWS_QCOUNT_LEN));
    return s+RWS_QCOUNT_INCR;
}
static inline uint64_t s_decr_qcount (uint64_t s) {
    //assert(s_get_qcount(s) > 0);
    return s-RWS_QCOUNT_INCR;
}
static inline uint64_t s_incr_rcount (uint64_t s) {
    //assert(s_get_rcount(s)+1 < (1u<<RWS_RCOUNT_LEN));
    return s+RWS_RCOUNT_INCR;
}
static inline uint64_t s_decr_rcount (uint64_t s) {
    //assert(s_get_rcount(s) > 0);
    return s-RWS_RCOUNT_INCR;
}

void toku_fair_rwlock_init (toku_fair_rwlock_t *rwlock);
void toku_fair_rwlock_destroy (toku_fair_rwlock_t *rwlock);
int toku_fair_rwlock_rdlock_slow (toku_fair_rwlock_t *rwlock);  // this is the slow internal version that grabs the mutex.

// Inline the fast path to avoid function call overhead.
static inline int toku_fair_rwlock_rdlock (toku_fair_rwlock_t *rwlock) {
    uint64_t s = rwlock->state;
 START:
    s = rwlock->state;
    if (0==(s&(RWS_QCOUNT_MASK | RWS_WLOCK_MASK))) goto C1;
    //if (s_get_qcount(s)==0 && !s_get_wlock(s)) goto C1;
    else goto ML;
 C1:
    if (toku_sync_bool_compare_and_swap(&rwlock->state, s, s_incr_rcount(s))) goto DONE;
    else goto START;
 DONE:
    return 0;
 ML:
    return toku_fair_rwlock_rdlock_slow(rwlock);
}

int toku_fair_rwlock_wrlock_slow (toku_fair_rwlock_t *rwlock);

// Inline the fast path to avoid function call overhead.
static inline int toku_fair_rwlock_wrlock (toku_fair_rwlock_t *rwlock) {
    uint64_t s;
 START:
    s = rwlock->state;
    if (s_get_qcount(s)==0 && !s_get_wlock(s) && s_get_rcount(s)==0) goto C1;
    else goto ML;
 C1:
    if (toku_sync_bool_compare_and_swap(&rwlock->state, s, s_set_wlock(s))) goto DONE;
    else goto START;
 DONE:
    return 0;
 ML:
    return toku_fair_rwlock_wrlock_slow(rwlock);
}	

int toku_fair_rwlock_unlock_r_slow (toku_fair_rwlock_t *rwlock);
int toku_fair_rwlock_unlock_w_slow (toku_fair_rwlock_t *rwlock);

static inline int toku_fair_rwlock_unlock (toku_fair_rwlock_t *rwlock) {
    uint64_t s;
    s = rwlock->state;
    if (s_get_wlock(s)) {
	goto wSTART0; // we already have s.
    wSTART:
	s = rwlock->state;
	goto wSTART0;
    wSTART0:
	if (s_get_qcount(s)==0) goto wC1;
	else goto wML;
    wC1:
	if (toku_sync_bool_compare_and_swap(&rwlock->state, s, s_clear_wlock(s))) goto wDONE;
	else goto wSTART;
    wDONE:
	return 0;
    wML:
	return toku_fair_rwlock_unlock_w_slow (rwlock);
    } else {
	goto rSTART0; // we already have s.
    rSTART:
	s = rwlock->state;
	goto rSTART0;
    rSTART0:
	if (s_get_rcount(s)>1 || s_get_qcount(s)==0) goto rC1;
	else goto rML;
    rC1:
	if (toku_sync_bool_compare_and_swap(&rwlock->state, s, s_decr_rcount(s))) goto rDONE;
	else goto rSTART;
    rDONE:
	return 0;
    rML:
	return toku_fair_rwlock_unlock_r_slow (rwlock);
    }
}	
int fcall_nop(int);
