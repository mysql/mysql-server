/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

/***********
 * The purpose of this file is to implement the high-level logic for 
 * taking a checkpoint.
 *
 * There are three locks used for taking a checkpoint.  They are listed below.
 *
 * NOTE: The reader-writer locks may be held by either multiple clients 
 *       or the checkpoint function.  (The checkpoint function has the role
 *       of the writer, the clients have the reader roles.)
 *
 *  - multi_operation_lock
 *    This is a new reader-writer lock.
 *    This lock is held by the checkpoint function only for as long as is required to 
 *    to set all the "pending" bits and to create the checkpoint-in-progress versions
 *    of the header and translation table (btt).
 *    The following operations must take the multi_operation_lock:
 *     - any set of operations that must be atomic with respect to begin checkpoint
 *
 *  - checkpoint_safe_lock
 *    This is a new reader-writer lock.
 *    This lock is held for the entire duration of the checkpoint.
 *    It is used to prevent more than one checkpoint from happening at a time
 *    (the checkpoint function is non-re-entrant), and to prevent certain operations
 *    that should not happen during a checkpoint.  
 *    The following operations must take the checkpoint_safe lock:
 *       - delete a dictionary
 *       - rename a dictionary
 *    The application can use this lock to disable checkpointing during other sensitive
 *    operations, such as making a backup copy of the database.
 *
 * Once the "pending" bits are set and the snapshots are taken of the header and btt,
 * most normal database operations are permitted to resume.
 *
 *
 *
 *****/

#include <toku_portability.h>
#include <time.h>

#include "fttypes.h"
#include "cachetable.h"
#include "log-internal.h"
#include "logger.h"
#include "checkpoint.h"
#include <portability/toku_atomic.h>
#include <util/status.h>

///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

static CHECKPOINT_STATUS_S cp_status;

#define STATUS_INIT(k,c,t,l,inc) TOKUDB_STATUS_INIT(cp_status, k, c, t, "checkpoint: " l, inc)

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.

    STATUS_INIT(CP_PERIOD,                              CHECKPOINT_PERIOD, UINT64,   "period", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CP_FOOTPRINT,                           nullptr, UINT64,   "footprint", TOKU_ENGINE_STATUS);
    STATUS_INIT(CP_TIME_LAST_CHECKPOINT_BEGIN,          CHECKPOINT_LAST_BEGAN, UNIXTIME, "last checkpoint began ", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CP_TIME_LAST_CHECKPOINT_BEGIN_COMPLETE, CHECKPOINT_LAST_COMPLETE_BEGAN, UNIXTIME, "last complete checkpoint began ", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CP_TIME_LAST_CHECKPOINT_END,            CHECKPOINT_LAST_COMPLETE_ENDED, UNIXTIME, "last complete checkpoint ended", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CP_LAST_LSN,                            nullptr, UINT64,   "last complete checkpoint LSN", TOKU_ENGINE_STATUS);
    STATUS_INIT(CP_CHECKPOINT_COUNT,                    CHECKPOINT_TAKEN, UINT64,   "checkpoints taken ", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CP_CHECKPOINT_COUNT_FAIL,               CHECKPOINT_FAILED, UINT64,   "checkpoints failed", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CP_WAITERS_NOW,                         nullptr, UINT64,   "waiters now", TOKU_ENGINE_STATUS);
    STATUS_INIT(CP_WAITERS_MAX,                         nullptr, UINT64,   "waiters max", TOKU_ENGINE_STATUS);
    STATUS_INIT(CP_CLIENT_WAIT_ON_MO,                   nullptr, UINT64,   "non-checkpoint client wait on mo lock", TOKU_ENGINE_STATUS);
    STATUS_INIT(CP_CLIENT_WAIT_ON_CS,                   nullptr, UINT64,   "non-checkpoint client wait on cs lock", TOKU_ENGINE_STATUS);

    STATUS_INIT(CP_BEGIN_TIME,                          CHECKPOINT_BEGIN_TIME, UINT64,   "checkpoint begin time", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CP_LONG_BEGIN_COUNT,                    CHECKPOINT_LONG_BEGIN_COUNT, UINT64,   "long checkpoint begin count", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CP_LONG_BEGIN_TIME,                     CHECKPOINT_LONG_BEGIN_TIME, UINT64,   "long checkpoint begin time", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);

    cp_status.initialized = true;
}
#undef STATUS_INIT

#define STATUS_VALUE(x) cp_status.status[x].value.num

void
toku_checkpoint_get_status(CACHETABLE ct, CHECKPOINT_STATUS statp) {
    if (!cp_status.initialized)
        status_init();
    STATUS_VALUE(CP_PERIOD) = toku_get_checkpoint_period_unlocked(ct);
    *statp = cp_status;
}



static LSN last_completed_checkpoint_lsn;

static toku_pthread_rwlock_t checkpoint_safe_lock;
static toku_pthread_rwlock_t multi_operation_lock;
static toku_pthread_rwlock_t low_priority_multi_operation_lock;

static bool initialized = false;     // sanity check
static volatile bool locked_mo = false;       // true when the multi_operation write lock is held (by checkpoint)
static volatile bool locked_cs = false;       // true when the checkpoint_safe write lock is held (by checkpoint)
static volatile uint64_t toku_checkpoint_long_threshold = 1000000;

// Note following static functions are called from checkpoint internal logic only,
// and use the "writer" calls for locking and unlocking.

static void
multi_operation_lock_init(void) {
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
#if defined(HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP)
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#else
    // TODO: need to figure out how to make writer-preferential rwlocks
    // happen on osx
#endif
    toku_pthread_rwlock_init(&multi_operation_lock, &attr);
    toku_pthread_rwlock_init(&low_priority_multi_operation_lock, &attr);
    pthread_rwlockattr_destroy(&attr);
    locked_mo = false;
}

static void
multi_operation_lock_destroy(void) {
    toku_pthread_rwlock_destroy(&multi_operation_lock);
    toku_pthread_rwlock_destroy(&low_priority_multi_operation_lock);
}

static void 
multi_operation_checkpoint_lock(void) {
    toku_pthread_rwlock_wrlock(&low_priority_multi_operation_lock);
    toku_pthread_rwlock_wrlock(&multi_operation_lock);   
    locked_mo = true;
}

static void 
multi_operation_checkpoint_unlock(void) {
    locked_mo = false;
    toku_pthread_rwlock_wrunlock(&multi_operation_lock);
    toku_pthread_rwlock_wrunlock(&low_priority_multi_operation_lock);    
}

static void
checkpoint_safe_lock_init(void) {
    toku_pthread_rwlock_init(&checkpoint_safe_lock, NULL); 
    locked_cs = false;
}

static void
checkpoint_safe_lock_destroy(void) {
    toku_pthread_rwlock_destroy(&checkpoint_safe_lock); 
}

static void 
checkpoint_safe_checkpoint_lock(void) {
    toku_pthread_rwlock_wrlock(&checkpoint_safe_lock);   
    locked_cs = true;
}

static void 
checkpoint_safe_checkpoint_unlock(void) {
    locked_cs = false;
    toku_pthread_rwlock_wrunlock(&checkpoint_safe_lock); 
}


// toku_xxx_client_(un)lock() functions are only called from client code,
// never from checkpoint code, and use the "reader" interface to the lock functions.

void 
toku_multi_operation_client_lock(void) {
    if (locked_mo)
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(CP_CLIENT_WAIT_ON_MO), 1);
    toku_pthread_rwlock_rdlock(&multi_operation_lock);   
}

void 
toku_multi_operation_client_unlock(void) {
    toku_pthread_rwlock_rdunlock(&multi_operation_lock); 
}

void toku_low_priority_multi_operation_client_lock(void) {
    toku_pthread_rwlock_rdlock(&low_priority_multi_operation_lock);
}

void toku_low_priority_multi_operation_client_unlock(void) {
    toku_pthread_rwlock_rdunlock(&low_priority_multi_operation_lock);
}

void 
toku_checkpoint_safe_client_lock(void) {
    if (locked_cs)
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(CP_CLIENT_WAIT_ON_CS), 1);
    toku_pthread_rwlock_rdlock(&checkpoint_safe_lock);  
    toku_multi_operation_client_lock();
}

void 
toku_checkpoint_safe_client_unlock(void) {
    toku_pthread_rwlock_rdunlock(&checkpoint_safe_lock); 
    toku_multi_operation_client_unlock();
}



// Initialize the checkpoint mechanism, must be called before any client operations.
void
toku_checkpoint_init(void) {
    multi_operation_lock_init();
    checkpoint_safe_lock_init();
    initialized = true;
}

void
toku_checkpoint_destroy(void) {
    multi_operation_lock_destroy();
    checkpoint_safe_lock_destroy();
    initialized = false;
}

#define SET_CHECKPOINT_FOOTPRINT(x) STATUS_VALUE(CP_FOOTPRINT) = footprint_offset + x


// Take a checkpoint of all currently open dictionaries
int 
toku_checkpoint(CHECKPOINTER cp, TOKULOGGER logger,
                void (*callback_f)(void*),  void * extra,
                void (*callback2_f)(void*), void * extra2,
                checkpoint_caller_t caller_id) {
    int footprint_offset = (int) caller_id * 1000;

    assert(initialized);

    (void) toku_sync_fetch_and_add(&STATUS_VALUE(CP_WAITERS_NOW), 1);
    checkpoint_safe_checkpoint_lock();
    (void) toku_sync_fetch_and_sub(&STATUS_VALUE(CP_WAITERS_NOW), 1);

    if (STATUS_VALUE(CP_WAITERS_NOW) > STATUS_VALUE(CP_WAITERS_MAX))
        STATUS_VALUE(CP_WAITERS_MAX) = STATUS_VALUE(CP_WAITERS_NOW);  // threadsafe, within checkpoint_safe lock

    SET_CHECKPOINT_FOOTPRINT(10);
    multi_operation_checkpoint_lock();
    SET_CHECKPOINT_FOOTPRINT(20);
    toku_ft_open_close_lock();
    
    SET_CHECKPOINT_FOOTPRINT(30);
    STATUS_VALUE(CP_TIME_LAST_CHECKPOINT_BEGIN) = time(NULL);
    uint64_t t_checkpoint_begin_start = toku_current_time_microsec();
    toku_cachetable_begin_checkpoint(cp, logger);
    uint64_t t_checkpoint_begin_end = toku_current_time_microsec();

    toku_ft_open_close_unlock();
    multi_operation_checkpoint_unlock();

    SET_CHECKPOINT_FOOTPRINT(40);
    if (callback_f) {
        callback_f(extra);      // callback is called with checkpoint_safe_lock still held
    }
    toku_cachetable_end_checkpoint(cp, logger, callback2_f, extra2);

    SET_CHECKPOINT_FOOTPRINT(50);
    if (logger) {
        last_completed_checkpoint_lsn = logger->last_completed_checkpoint_lsn;
        toku_logger_maybe_trim_log(logger, last_completed_checkpoint_lsn);
        STATUS_VALUE(CP_LAST_LSN) = last_completed_checkpoint_lsn.lsn;
    }

    SET_CHECKPOINT_FOOTPRINT(60);
    STATUS_VALUE(CP_TIME_LAST_CHECKPOINT_END) = time(NULL);
    STATUS_VALUE(CP_TIME_LAST_CHECKPOINT_BEGIN_COMPLETE) = STATUS_VALUE(CP_TIME_LAST_CHECKPOINT_BEGIN);
    STATUS_VALUE(CP_CHECKPOINT_COUNT)++;
    uint64_t duration = t_checkpoint_begin_end - t_checkpoint_begin_start;
    STATUS_VALUE(CP_BEGIN_TIME) += duration;
    if (duration >= toku_checkpoint_long_threshold) {
        STATUS_VALUE(CP_LONG_BEGIN_TIME) += duration;
        STATUS_VALUE(CP_LONG_BEGIN_COUNT) += 1;
    }
    STATUS_VALUE(CP_FOOTPRINT) = 0;

    checkpoint_safe_checkpoint_unlock();
    return 0;
}

#include <toku_race_tools.h>
void __attribute__((__constructor__)) toku_checkpoint_helgrind_ignore(void);
void
toku_checkpoint_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&cp_status, sizeof cp_status);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&locked_mo, sizeof locked_mo);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&locked_cs, sizeof locked_cs);
}

#undef SET_CHECKPOINT_FOOTPRINT
#undef STATUS_VALUE
