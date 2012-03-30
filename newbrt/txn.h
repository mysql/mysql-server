/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef TOKUTXN_H
#define TOKUTXN_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

int toku_txn_begin_txn (
    DB_TXN  *container_db_txn,
    TOKUTXN parent_tokutxn, 
    TOKUTXN *tokutxn, 
    TOKULOGGER logger,
    TXN_SNAPSHOT_TYPE snapshot_type
    );

DB_TXN * toku_txn_get_container_db_txn (TOKUTXN tokutxn);
void toku_txn_set_container_db_txn (TOKUTXN, DB_TXN*);

// toku_txn_begin_with_xid is called from recovery and has no containing DB_TXN 
int toku_txn_begin_with_xid (
    TOKUTXN parent_tokutxn, 
    TOKUTXN *tokutxn, 
    TOKULOGGER logger, 
    TXNID xid, 
    TXN_SNAPSHOT_TYPE snapshot_type,
    DB_TXN *container_db_txn
    );

// Allocate and initialize a txn
int toku_txn_create_txn(TOKUTXN *txn_ptr, TOKUTXN parent, TOKULOGGER logger, TXNID xid, TXN_SNAPSHOT_TYPE snapshot_type, DB_TXN *container_db_txn);

// Assign a txnid. Log the txn begin in the recovery log. Initialize the txn live lists.
int toku_txn_start_txn(TOKUTXN txn);

int toku_txn_load_txninfo (TOKUTXN txn, TXNINFO info);

int toku_txn_commit_txn (TOKUTXN txn, int nosync, YIELDF yield, void *yieldv,
			 TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra,
			 bool release_multi_operation_client_lock);
BOOL toku_txn_requires_checkpoint(TOKUTXN txn);
int toku_txn_commit_with_lsn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv, LSN oplsn,
			     TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra,
			     bool release_multi_operation_client_lock);

int toku_txn_abort_txn(TOKUTXN txn, YIELDF yield, void *yieldv,
                       TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra,
		       bool release_multi_operation_client_lock);
int toku_txn_abort_with_lsn(TOKUTXN txn, YIELDF yield, void *yieldv, LSN oplsn,
                            TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra,
			    bool release_multi_operation_client_lock);

int toku_txn_prepare_txn (TOKUTXN txn, GID gid) __attribute__((warn_unused_result));
// Effect: Do the internal work of preparing a transaction (does not log the prepare record).

void toku_txn_get_prepared_gid (TOKUTXN, GID *);
// Effect: Return a pointer to the GID.  The value is allocated, so you must free it.

int toku_txn_maybe_fsync_log(TOKULOGGER logger, LSN do_fsync_lsn, BOOL do_fsync, YIELDF yield, void *yieldv);

void toku_txn_get_fsync_info(TOKUTXN ttxn, BOOL* do_fsync, LSN* do_fsync_lsn);

// Complete and destroy a txn
void toku_txn_close_txn(TOKUTXN txn);

// Remove a txn from any live txn lists
void toku_txn_complete_txn(TOKUTXN txn);

// Free the memory of a txn
void toku_txn_destroy_txn(TOKUTXN txn);

XIDS toku_txn_get_xids (TOKUTXN);

// Returns TRUE if a is older than b
BOOL toku_txnid_older(TXNID a, TXNID b);

// Returns TRUE if a == b
BOOL toku_txnid_eq(TXNID a, TXNID b);

// Returns TRUE if a is newer than b
BOOL toku_txnid_newer(TXNID a, TXNID b);

// Force fsync on commit
void toku_txn_force_fsync_on_commit(TOKUTXN txn);

typedef enum {
    TXN_BEGIN,             // total number of transactions begun (does not include recovered txns)
    TXN_COMMIT,            // successful commits
    TXN_ABORT,
    TXN_CLOSE,             // should be sum of aborts and commits
    TXN_NUM_OPEN,          // should be begin - close
    TXN_MAX_OPEN,          // max value of num_open
    TXN_OLDEST_LIVE,       // xid of oldest live transaction
    TXN_OLDEST_STARTTIME,  // start time of oldest live txn
    TXN_STATUS_NUM_ROWS
} txn_status_entry;

typedef struct {
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[TXN_STATUS_NUM_ROWS];
} TXN_STATUS_S, *TXN_STATUS;

void toku_txn_get_status(TOKULOGGER logger, TXN_STATUS s);

BOOL toku_is_txn_in_live_root_txn_list(TOKUTXN txn, TXNID xid);

TXNID toku_get_oldest_in_live_root_txn_list(TOKUTXN txn);

typedef struct {
    TXNID xid1;
    TXNID xid2;
} XID_PAIR_S, *XID_PAIR;

// 2954
typedef struct tokutxn_filenum_ignore_errors {
    uint32_t fns_allocated;
    FILENUMS filenums;
} TXN_IGNORE_S, *TXN_IGNORE;

void toku_txn_ignore_init(TOKUTXN txn);
void toku_txn_ignore_free(TOKUTXN txn);
int  toku_txn_ignore_add(TOKUTXN txn, FILENUM filenum);
int  toku_txn_ignore_remove(TOKUTXN txn, FILENUM filenum);
int  toku_txn_ignore_contains(TOKUTXN txn, FILENUM filenum);

enum tokutxn_state {
    TOKUTXN_LIVE,         // initial txn state
    TOKUTXN_PREPARING,    // txn is preparing (or prepared)
    TOKUTXN_COMMITTING,   // txn in the process of committing
    TOKUTXN_ABORTING,     // txn in the process of aborting
    TOKUTXN_RETIRED,      // txn no longer exists
};
typedef enum tokutxn_state TOKUTXN_STATE;

TOKUTXN_STATE toku_txn_get_state(TOKUTXN txn);

int toku_logger_recover_txn (TOKULOGGER logger, DB_PREPLIST preplist[/*count*/], long count, /*out*/ long *retp, u_int32_t flags);

#if defined(__cplusplus) || defined(__cilkplusplus)
}
#endif

#endif //TOKUTXN_H
