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

// toku_txn_begin_with_xid is called from recovery and has no containing DB_TXN 
int toku_txn_begin_with_xid (
    TOKUTXN parent_tokutxn, 
    TOKUTXN *tokutxn, 
    TOKULOGGER logger, 
    TXNID xid, 
    TXN_SNAPSHOT_TYPE snapshot_type
    );

int toku_txn_load_txninfo (TOKUTXN txn, TXNINFO info);

int toku_txn_commit_txn (TOKUTXN txn, int nosync, YIELDF yield, void *yieldv,
			 TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);
BOOL toku_txn_requires_checkpoint(TOKUTXN txn);
int toku_txn_commit_with_lsn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv, LSN oplsn,
			     TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);

int toku_txn_abort_txn(TOKUTXN txn, YIELDF yield, void *yieldv,
                       TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);
int toku_txn_abort_with_lsn(TOKUTXN txn, YIELDF yield, void *yieldv, LSN oplsn,
                            TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);

int toku_txn_maybe_fsync_log(TOKUTXN txn, YIELDF yield, void *yieldv);
void toku_txn_close_txn(TOKUTXN txn);

XIDS toku_txn_get_xids (TOKUTXN);

// Returns TRUE if a is older than b
BOOL toku_txnid_older(TXNID a, TXNID b);

// Returns TRUE if a == b
BOOL toku_txnid_eq(TXNID a, TXNID b);

// Returns TRUE if a is newer than b
BOOL toku_txnid_newer(TXNID a, TXNID b);

// Force fsync on commit
void toku_txn_force_fsync_on_commit(TOKUTXN txn);


typedef struct txn_status {
    u_int64_t   begin;       // total number of transactions begun (does not include recovered txns)
    u_int64_t   commit;      // successful commits
    u_int64_t   abort;
    u_int64_t   close;       // should be sum of aborts and commits
    u_int64_t   num_open;    // should be begin - close
    u_int64_t   max_open;    // max value of num_open
} TXN_STATUS_S, *TXN_STATUS;

void toku_txn_get_status(TXN_STATUS s);

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

int  toku_txn_ignore_init(TOKUTXN txn);
void toku_txn_ignore_free(TOKUTXN txn);
int  toku_txn_ignore_add(TOKUTXN txn, FILENUM filenum);
int  toku_txn_ignore_remove(TOKUTXN txn, FILENUM filenum);
int  toku_txn_ignore_contains(TOKUTXN txn, FILENUM filenum);

enum tokutxn_state {
    TOKUTXN_LIVE,         // initial txn state
    TOKUTXN_COMMITTING,   // txn in the process of committing
    TOKUTXN_ABORTING,     // txn in the process of aborting
    TOKUTXN_RETIRED,      // txn no longer exists
};
typedef enum tokutxn_state TOKUTXN_STATE;

TOKUTXN_STATE toku_txn_get_state(TOKUTXN txn);

#if defined(__cplusplus) || defined(__cilkplusplus)
}
#endif

#endif //TOKUTXN_H
