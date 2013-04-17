/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "txn.h"

int toku_txn_begin_txn (TOKUTXN parent_tokutxn, TOKUTXN *tokutxn, TOKULOGGER logger) {
    return toku_txn_begin_with_xid(parent_tokutxn, tokutxn, logger, 0);
}

int toku_txn_begin_with_xid (TOKUTXN parent_tokutxn, TOKUTXN *tokutxn, TOKULOGGER logger, TXNID xid) {
    if (logger->is_panicked) return EINVAL;
    TAGMALLOC(TOKUTXN, result);
    if (result==0) 
        return errno;
    int r;
    if (xid == 0) {
        r = toku_log_xbegin(logger, &result->first_lsn, 0, parent_tokutxn ? parent_tokutxn->txnid64 : 0);
        if (r!=0) goto died;
    } else
        result->first_lsn.lsn = xid;
    r = toku_omt_create(&result->open_brts);
    if (r!=0) goto died;
    result->txnid64 = result->first_lsn.lsn;
    XIDS parent_xids;
    if (parent_tokutxn==NULL)
        parent_xids = xids_get_root_xids();
    else
        parent_xids = parent_tokutxn->xids;
    if ((r=xids_create_child(parent_xids, &result->xids, result->txnid64)))
        goto died;
    result->logger = logger;
    result->parent = parent_tokutxn;
    result->oldest_logentry = result->newest_logentry = 0;

    result->rollentry_arena = memarena_create();
    result->num_rollentries = 0;
    result->num_rollentries_processed = 0;
    result->progress_poll_fun = NULL;
    result->progress_poll_fun_extra = NULL;

    if (toku_omt_size(logger->live_txns) == 0) {
        assert(logger->oldest_living_xid == TXNID_NONE_LIVING);
        logger->oldest_living_xid = result->txnid64;
    }
    assert(logger->oldest_living_xid <= result->txnid64);

    {
        //Add txn to list (omt) of live transactions
        u_int32_t idx;
        r = toku_omt_insert(logger->live_txns, result, find_xid, result, &idx);
        if (r!=0) goto died;

        if (logger->oldest_living_xid == result->txnid64)
            assert(idx == 0);
        else
            assert(idx > 0);
    }

    result->rollentry_resident_bytecount=0;
    result->rollentry_raw_count = 0;
    result->rollentry_filename = 0;
    result->rollentry_fd = -1;
    result->rollentry_filesize = 0;
    result->force_fsync_on_commit = FALSE;
    result->has_done_work = 0;
    *tokutxn = result;
    return 0;

died:
    // TODO memory leak
    toku_logger_panic(logger, r);
    return r; 
}


// Doesn't close the txn, just performs the commit operations.
int toku_txn_commit_txn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv,
                        TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra,
			void (*release_locks)(void*), void(*reacquire_locks)(void*), void *locks_thunk) {
    return toku_txn_commit_with_lsn(txn, nosync, yield, yieldv, ZERO_LSN,
				    poll, poll_extra,
				    release_locks, reacquire_locks, locks_thunk);
}

int toku_txn_commit_with_lsn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv, LSN oplsn,
                             TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra,
			     void (*release_locks)(void*), void(*reacquire_locks)(void*), void *locks_thunk) {
    int r;
    // panic handled in log_commit

    //Child transactions do not actually 'commit'.  They promote their changes to parent, so no need to fsync if this txn has a parent.
    int do_fsync = !txn->parent && (txn->force_fsync_on_commit || (!nosync && txn->has_done_work));

    txn->progress_poll_fun = poll;
    txn->progress_poll_fun_extra = poll_extra;

    if (release_locks) release_locks(locks_thunk);
    r = toku_log_commit(txn->logger, (LSN*)0, do_fsync, txn->txnid64); // exits holding neither of the tokulogger locks.
    if (reacquire_locks) reacquire_locks(locks_thunk);
    if (r!=0)
        return r;
    r = toku_rollback_commit(txn, yield, yieldv, oplsn);
    return r;
}

// Doesn't close the txn, just performs the abort operations.
int toku_txn_abort_txn(TOKUTXN txn, YIELDF yield, void *yieldv,
                       TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) {
    return toku_txn_abort_with_lsn(txn, yield, yieldv, ZERO_LSN, poll, poll_extra);
}

int toku_txn_abort_with_lsn(TOKUTXN txn, YIELDF yield, void *yieldv, LSN oplsn,
                            TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) {
    //printf("%s:%d aborting\n", __FILE__, __LINE__);
    // Must undo everything.  Must undo it all in reverse order.
    // Build the reverse list
    //printf("%s:%d abort\n", __FILE__, __LINE__);
    txn->progress_poll_fun = poll;
    txn->progress_poll_fun_extra = poll_extra;
    int r=0;
    r = toku_log_xabort(txn->logger, (LSN*)0, 0, txn->txnid64);
    if (r!=0) 
        return r;
    r = toku_rollback_abort(txn, yield, yieldv, oplsn);
    return r;
}

void toku_txn_close_txn(TOKUTXN txn) {
    toku_rollback_txn_close(txn);
    return;
}

XIDS toku_txn_get_xids (TOKUTXN txn) {
    if (txn==0) return xids_get_root_xids();
    else return txn->xids;
}

BOOL toku_txnid_older(TXNID a, TXNID b) {
    return (BOOL)(a < b); // TODO need modulo 64 arithmetic
}

BOOL toku_txnid_newer(TXNID a, TXNID b) {
    return (BOOL)(a > b); // TODO need modulo 64 arithmetic
}

BOOL toku_txnid_eq(TXNID a, TXNID b) {
    return (BOOL)(a == b);
}

void toku_txn_force_fsync_on_commit(TOKUTXN txn) {
    txn->force_fsync_on_commit = TRUE;
}
