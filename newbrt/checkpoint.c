/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009-2010 Tokutek Inc.  All rights reserved."
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
 *       - insertion into multiple indexes
 *       - "replace-into" (matching delete and insert on a single key)
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
 *  - ydb_big_lock
 *    This is the existing lock used to serialize all access to tokudb.
 *    This lock is held by the checkpoint function only for as long as is required to 

 *    to set all the "pending" bits and to create the checkpoint-in-progress versions
 *    of the header and translation table (btt).
 *    
 * Once the "pending" bits are set and the snapshots are taken of the header and btt,
 * most normal database operations are permitted to resume.
 *
 *
 *
 *****/

#include <toku_portability.h>
#include <time.h>

#include "brttypes.h"
#include "cachetable.h"
#include "log-internal.h"
#include "logger.h"
#include "checkpoint.h"

static CHECKPOINT_STATUS_S cp_status;
static LSN last_completed_checkpoint_lsn;

static toku_pthread_rwlock_t checkpoint_safe_lock;
static toku_pthread_rwlock_t multi_operation_lock;

// Call through function pointers because this layer has no access to ydb lock functions.
static void (*ydb_lock)(void)   = NULL;
static void (*ydb_unlock)(void) = NULL;

static BOOL initialized = FALSE;     // sanity check
static BOOL locked_mo = FALSE;       // true when the multi_operation write lock is held (by checkpoint)
static BOOL locked_cs = FALSE;       // true when the checkpoint_safe write lock is held (by checkpoint)


// Note following static functions are called from checkpoint internal logic only,
// and use the "writer" calls for locking and unlocking.


static int 
multi_operation_lock_init(void) {
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    int r = toku_pthread_rwlock_init(&multi_operation_lock, &attr); 
    pthread_rwlockattr_destroy(&attr);
    assert(r == 0);
    locked_mo = FALSE;
    return r;
}

static int 
multi_operation_lock_destroy(void) {
    int r = toku_pthread_rwlock_destroy(&multi_operation_lock); 
    assert(r == 0);
    return r;
}

static void 
multi_operation_checkpoint_lock(void) {
    int r = toku_pthread_rwlock_wrlock(&multi_operation_lock);   
    assert(r == 0);
    locked_mo = TRUE;
}

static void 
multi_operation_checkpoint_unlock(void) {
    locked_mo = FALSE;
    int r = toku_pthread_rwlock_wrunlock(&multi_operation_lock); 
    assert(r == 0);
}


static int 
checkpoint_safe_lock_init(void) {
    int r = toku_pthread_rwlock_init(&checkpoint_safe_lock, NULL); 
    assert(r == 0);
    locked_cs = FALSE;
    return r;
}

static int 
checkpoint_safe_lock_destroy(void) {
    int r = toku_pthread_rwlock_destroy(&checkpoint_safe_lock); 
    assert(r == 0);
    return r;
}

static void 
checkpoint_safe_checkpoint_lock(void) {
    int r = toku_pthread_rwlock_wrlock(&checkpoint_safe_lock);   
    assert(r == 0);
    locked_cs = TRUE;
}

static void 
checkpoint_safe_checkpoint_unlock(void) {
    locked_cs = FALSE;
    int r = toku_pthread_rwlock_wrunlock(&checkpoint_safe_lock); 
    assert(r == 0);
}


// toku_xxx_client_(un)lock() functions are only called from client code,
// never from checkpoint code, and use the "reader" interface to the lock functions.

void 
toku_multi_operation_client_lock(void) {
    if (locked_mo)
	(void) __sync_fetch_and_add(&cp_status.client_wait_on_mo, 1);
    int r = toku_pthread_rwlock_rdlock(&multi_operation_lock);   
    assert(r == 0);
}

void 
toku_multi_operation_client_unlock(void) {
    int r = toku_pthread_rwlock_rdunlock(&multi_operation_lock); 
    assert(r == 0);
}

void 
toku_checkpoint_safe_client_lock(void) {
    if (locked_cs)
	(void) __sync_fetch_and_add(&cp_status.client_wait_on_cs, 1);
    int r = toku_pthread_rwlock_rdlock(&checkpoint_safe_lock);  
    assert(r == 0);
    toku_multi_operation_client_lock();
}

void 
toku_checkpoint_safe_client_unlock(void) {
    int r = toku_pthread_rwlock_rdunlock(&checkpoint_safe_lock); 
    assert(r == 0);
    toku_multi_operation_client_unlock();
}


void
toku_checkpoint_get_status(CHECKPOINT_STATUS s) {
    *s = cp_status;
}



// Initialize the checkpoint mechanism, must be called before any client operations.
int 
toku_checkpoint_init(void (*ydb_lock_callback)(void), void (*ydb_unlock_callback)(void)) {
    int r = 0;
    ydb_lock   = ydb_lock_callback;
    ydb_unlock = ydb_unlock_callback;
    if (r==0)
        r = multi_operation_lock_init();
    if (r==0)
        r = checkpoint_safe_lock_init();
    if (r==0)
        initialized = TRUE;
    return r;
}

int
toku_checkpoint_destroy(void) {
    int r = 0;
    if (r==0)
        r = multi_operation_lock_destroy();
    if (r==0)
        r = checkpoint_safe_lock_destroy();
    initialized = FALSE;
    return r;
}

#define SET_CHECKPOINT_FOOTPRINT(x) cp_status.footprint = footprint_offset + x


// Take a checkpoint of all currently open dictionaries
int 
toku_checkpoint(CACHETABLE ct, TOKULOGGER logger,
		void (*callback_f)(void*),  void * extra,
		void (*callback2_f)(void*), void * extra2,
		checkpoint_caller_t caller_id) {
    int r;
    int footprint_offset = (int) caller_id * 1000;

    assert(initialized);

    if (locked_cs) {
	if (caller_id == SCHEDULED_CHECKPOINT)
	    (void) __sync_fetch_and_add(&cp_status.cp_wait_sched_cs, 1);
	else if (caller_id == CLIENT_CHECKPOINT)
	    (void) __sync_fetch_and_add(&cp_status.cp_wait_client_cs, 1);
	else if (caller_id == TXN_COMMIT_CHECKPOINT)
	    (void) __sync_fetch_and_add(&cp_status.cp_wait_txn_cs, 1);
	else 
	    (void) __sync_fetch_and_add(&cp_status.cp_wait_other_cs, 1);
    }

    (void) __sync_fetch_and_add(&cp_status.waiters_now, 1);
    checkpoint_safe_checkpoint_lock();
    (void) __sync_fetch_and_sub(&cp_status.waiters_now, 1);

    if (cp_status.waiters_now > cp_status.waiters_max)
	cp_status.waiters_max = cp_status.waiters_now;  // threadsafe, within checkpoint_safe lock

    SET_CHECKPOINT_FOOTPRINT(10);
    if (locked_mo) {
	if (caller_id == SCHEDULED_CHECKPOINT)
	    cp_status.cp_wait_sched_mo++;           // threadsafe, within checkpoint_safe lock
	else if (caller_id == CLIENT_CHECKPOINT)
	    cp_status.cp_wait_client_mo++;
	else if (caller_id == TXN_COMMIT_CHECKPOINT)
	    cp_status.cp_wait_txn_mo++;
	else 
	    cp_status.cp_wait_other_mo++;
    }
    multi_operation_checkpoint_lock();
    SET_CHECKPOINT_FOOTPRINT(20);
    ydb_lock();
    
    SET_CHECKPOINT_FOOTPRINT(30);
    cp_status.time_last_checkpoint_begin = time(NULL);
    r = toku_cachetable_begin_checkpoint(ct, logger);

    multi_operation_checkpoint_unlock();
    ydb_unlock();

    SET_CHECKPOINT_FOOTPRINT(40);
    if (r==0) {
	if (callback_f) 
	    callback_f(extra);      // callback is called with checkpoint_safe_lock still held
	r = toku_cachetable_end_checkpoint(ct, logger, ydb_lock, ydb_unlock, callback2_f, extra2);
    }
    SET_CHECKPOINT_FOOTPRINT(50);
    if (r==0 && logger) {
        last_completed_checkpoint_lsn = logger->last_completed_checkpoint_lsn;
        r = toku_logger_maybe_trim_log(logger, last_completed_checkpoint_lsn);
	cp_status.last_lsn                   = last_completed_checkpoint_lsn.lsn;
    }

    SET_CHECKPOINT_FOOTPRINT(60);
    cp_status.time_last_checkpoint_end = time(NULL);
    cp_status.time_last_checkpoint_begin_complete = cp_status.time_last_checkpoint_begin;

    if (r == 0)
	cp_status.checkpoint_count++;
    else
	cp_status.checkpoint_count_fail++;

    cp_status.footprint = 0;
    checkpoint_safe_checkpoint_unlock();
    return r;
}

#undef SET_CHECKPOINT_FOOTPRINT


#include <valgrind/drd.h>
void __attribute__((__constructor__)) toku_checkpoint_drd_ignore(void);
void
toku_checkpoint_drd_ignore(void) {
    DRD_IGNORE_VAR(cp_status);
}
