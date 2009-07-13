/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: txn.c 12375 2009-05-28 14:14:47Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "txn.h"

int toku_txn_begin_txn (TOKUTXN parent_tokutxn, TOKUTXN *tokutxn, TOKULOGGER logger) {
    if (logger->is_panicked) return EINVAL;
    TAGMALLOC(TOKUTXN, result);
    if (result==0) 
        return errno;
    int r;
    r = toku_log_xbegin(logger, &result->first_lsn, 0, parent_tokutxn ? parent_tokutxn->txnid64 : 0);
    if (r!=0) { 
        toku_logger_panic(logger, r);  
        toku_free(result); 
        return r; 
    }
    r = toku_omt_create(&result->open_brts);
    if (r!=0) {
	toku_logger_panic(logger, r);
	toku_free(result);
	return r;
    }
    result->txnid64 = result->first_lsn.lsn;
    result->logger = logger;
    result->parent = parent_tokutxn;
    result->oldest_logentry = result->newest_logentry = 0;

    result->rollentry_arena = memarena_create();

    list_push(&logger->live_txns, &result->live_txns_link);
    result->rollentry_resident_bytecount=0;
    result->rollentry_raw_count = 0;
    result->rollentry_filename = 0;
    result->rollentry_fd = -1;
    result->rollentry_filesize = 0;
    *tokutxn = result;
    return 0;
}

// Doesn't close the txn, just performs the commit operations.
int toku_txn_commit_txn (TOKUTXN txn, int nosync, YIELDF yield, void*yieldv) {
    int r;
    // panic handled in log_commit
    r = toku_log_commit(txn->logger, (LSN*)0, (txn->parent==0) && !nosync, txn->txnid64); // exits holding neither of the tokulogger locks.
    if (r!=0)
        return r;
    r = toku_rollback_commit(txn, yield, yieldv);
    return r;
}

// Doesn't close the txn, just performs the abort operations.
int toku_txn_abort_txn(TOKUTXN txn, YIELDF yield, void*yieldv) {
    //printf("%s:%d aborting\n", __FILE__, __LINE__);
    // Must undo everything.  Must undo it all in reverse order.
    // Build the reverse list
    //printf("%s:%d abort\n", __FILE__, __LINE__);
    int r=0;
    r = toku_log_xabort(txn->logger, (LSN*)0, 0, txn->txnid64);
    if (r!=0) 
        return r;
    r = toku_rollback_abort(txn, yield, yieldv);
    return r;
}

void toku_txn_close_txn(TOKUTXN txn) {
    toku_rollback_txn_close(txn);
    return;
}
