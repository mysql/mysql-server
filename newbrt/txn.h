#ifndef TOKUTXN_H
#define TOKUTXN_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

int toku_txn_begin_txn (TOKUTXN parent_tokutxn, TOKUTXN *tokutxn, TOKULOGGER logger);
int toku_txn_begin_with_xid (TOKUTXN parent_tokutxn, TOKUTXN *tokutxn, TOKULOGGER logger, TXNID xid);
int toku_txn_load_txninfo (TOKUTXN txn, TXNINFO info);

int toku_txn_commit_txn (TOKUTXN txn, int nosync, YIELDF yield, void *yieldv,
			 TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);
int toku_txn_commit_with_lsn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv, LSN oplsn,
			     TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);

int toku_txn_abort_txn(TOKUTXN txn, YIELDF yield, void *yieldv,
                       TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);
int toku_txn_abort_with_lsn(TOKUTXN txn, YIELDF yield, void *yieldv, LSN oplsn,
                            TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);

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

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif //TOKUTXN_H
