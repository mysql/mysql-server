/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKU_CHECKPOINT_H
#define TOKU_CHECKPOINT_H

#ident "Copyright (c) 2009-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"


int toku_set_checkpoint_period(CACHETABLE ct, uint32_t new_period);
//Effect: Change [end checkpoint (n) - begin checkpoint (n+1)] delay to
//        new_period seconds.  0 means disable.

uint32_t toku_get_checkpoint_period(CACHETABLE ct);
uint32_t toku_get_checkpoint_period_unlocked(CACHETABLE ct);


/******
 *
 * NOTE: checkpoint_safe_lock is highest level lock
 *       multi_operation_lock is next level lock
 *       ydb_big_lock is next level lock
 *
 *       Locks must always be taken in this sequence (highest level first).
 *
 */


/****** 
 * Client code must hold the checkpoint_safe lock during the following operations:
 *       - delete a dictionary via DB->remove
 *       - delete a dictionary via DB_TXN->abort(txn) (where txn created a dictionary)
 *       - rename a dictionary //TODO: Handlerton rename needs to take this
 *                             //TODO: Handlerton rename needs to be recoded for transaction recovery
 *****/

void toku_checkpoint_safe_client_lock(void);

void toku_checkpoint_safe_client_unlock(void);



/****** 
 * These functions are called from the ydb level.
 * Client code must hold the multi_operation lock during the following operations:
 *       - insertion into multiple indexes
 *       - replace into (simultaneous delete/insert on a single key)
 *****/

void toku_multi_operation_client_lock(void);

void toku_multi_operation_client_unlock(void);


// Initialize the checkpoint mechanism, must be called before any client operations.
// Must pass in function pointers to take/release ydb lock.
void toku_checkpoint_init(void);

void toku_checkpoint_destroy(void);

typedef enum {SCHEDULED_CHECKPOINT  = 0,   // "normal" checkpoint taken on checkpoint thread
	      CLIENT_CHECKPOINT     = 1,   // induced by client, such as FLUSH LOGS or SAVEPOINT
	      TXN_COMMIT_CHECKPOINT = 2,   
	      STARTUP_CHECKPOINT    = 3, 
	      UPGRADE_CHECKPOINT    = 4,
	      RECOVERY_CHECKPOINT   = 5,
	      SHUTDOWN_CHECKPOINT   = 6} checkpoint_caller_t;

// Take a checkpoint of all currently open dictionaries
// Callbacks are called during checkpoint procedure while checkpoint_safe lock is still held.
// Callbacks are primarily intended for use in testing.
// caller_id identifies why the checkpoint is being taken.
int toku_checkpoint(CACHETABLE ct, TOKULOGGER logger,
		    void (*callback_f)(void*),  void * extra,
		    void (*callback2_f)(void*), void * extra2,
		    checkpoint_caller_t caller_id);



/******
 * These functions are called from the ydb level.
 * They return status information and have no side effects.
 * Some status information may be incorrect because no locks are taken to collect status.
 * (If checkpoint is in progress, it may overwrite status info while it is being read.)
 *****/
typedef enum {
    CP_PERIOD,
    CP_FOOTPRINT,
    CP_TIME_LAST_CHECKPOINT_BEGIN,
    CP_TIME_LAST_CHECKPOINT_BEGIN_COMPLETE,
    CP_TIME_LAST_CHECKPOINT_END,
    CP_LAST_LSN,
    CP_CHECKPOINT_COUNT,
    CP_CHECKPOINT_COUNT_FAIL,
    CP_WAITERS_NOW,          // how many threads are currently waiting for the checkpoint_safe lock to perform a checkpoint
    CP_WAITERS_MAX,          // max threads ever simultaneously waiting for the checkpoint_safe lock to perform a checkpoint
    CP_CLIENT_WAIT_ON_MO,    // how many times a client thread waited to take the multi_operation lock, not for checkpoint
    CP_CLIENT_WAIT_ON_CS,    // how many times a client thread waited for the checkpoint_safe lock, not for checkpoint
    CP_WAIT_SCHED_CS,        // how many times a scheduled checkpoint waited for the checkpoint_safe lock
    CP_WAIT_CLIENT_CS,       // how many times a client checkpoint waited for the checkpoint_safe lock
    CP_WAIT_TXN_CS,          // how many times a txn_commit checkpoint waited for the checkpoint_safe lock
    CP_WAIT_OTHER_CS,        // how many times a checkpoint for another purpose waited for the checkpoint_safe lock
    CP_WAIT_SCHED_MO,        // how many times a scheduled checkpoint waited for the multi_operation lock
    CP_WAIT_CLIENT_MO,       // how many times a client checkpoint waited for the multi_operation lock
    CP_WAIT_TXN_MO,          // how many times a txn_commit checkpoint waited for the multi_operation lock
    CP_WAIT_OTHER_MO,        // how many times a checkpoint for another purpose waited for the multi_operation lock
    CP_STATUS_NUM_ROWS       // number of rows in this status array
} cp_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[CP_STATUS_NUM_ROWS];
} CHECKPOINT_STATUS_S, *CHECKPOINT_STATUS;

void toku_checkpoint_get_status(CACHETABLE ct, CHECKPOINT_STATUS stat);


#endif
