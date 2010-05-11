/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#include "includes.h"
#include "txn.h"
#include "checkpoint.h"


// accountability
static TXN_STATUS_S status = {.begin  = 0, 
                              .commit = 0,
                              .abort  = 0,
                              .close  = 0};


void 
toku_txn_get_status(TXN_STATUS s) {
    *s = status;
}

int toku_txn_begin_txn (TOKUTXN parent_tokutxn, TOKUTXN *tokutxn, TOKULOGGER logger) {
    return toku_txn_begin_with_xid(parent_tokutxn, tokutxn, logger, 0);
}

int toku_txn_begin_with_xid (TOKUTXN parent_tokutxn, TOKUTXN *tokutxn, TOKULOGGER logger, TXNID xid) {
    if (logger->is_panicked) return EINVAL;
    assert(logger->rollback_cachefile);
    TAGMALLOC(TOKUTXN, result);
    if (result==0) 
        return errno;
    int r;
    LSN first_lsn;
    if (xid == 0) {
        r = toku_log_xbegin(logger, &first_lsn, 0, parent_tokutxn ? parent_tokutxn->txnid64 : 0);
        if (r!=0) goto died;
    } else
        first_lsn.lsn = xid;
    r = toku_omt_create(&result->open_brts);
    if (r!=0) goto died;
    result->txnid64 = first_lsn.lsn;
    XIDS parent_xids;
    if (parent_tokutxn==NULL)
        parent_xids = xids_get_root_xids();
    else
        parent_xids = parent_tokutxn->xids;
    if ((r=xids_create_child(parent_xids, &result->xids, result->txnid64)))
        goto died;
    result->logger = logger;
    result->parent = parent_tokutxn;

    result->num_rollentries = 0;
    result->num_rollentries_processed = 0;
    result->progress_poll_fun = NULL;
    result->progress_poll_fun_extra = NULL;
    result->spilled_rollback_head      = ROLLBACK_NONE;
    result->spilled_rollback_tail      = ROLLBACK_NONE;
    result->spilled_rollback_head_hash = 0;
    result->spilled_rollback_tail_hash = 0;
    result->current_rollback      = ROLLBACK_NONE;
    result->current_rollback_hash = 0;
    result->num_rollback_nodes = 0;
    result->pinned_inprogress_rollback_log = NULL;

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

    result->rollentry_raw_count = 0;
    result->force_fsync_on_commit = FALSE;
    result->recovered_from_checkpoint = FALSE;
    toku_list_init(&result->checkpoint_before_commit);
    *tokutxn = result;
    status.begin++;
    return 0;

died:
    // TODO memory leak
    toku_logger_panic(logger, r);
    return r; 
}

//Used on recovery to recover a transaction.
int
toku_txn_load_txninfo (TOKUTXN txn, TXNINFO info) {
#define COPY_FROM_INFO(field) txn->field = info->field
    COPY_FROM_INFO(rollentry_raw_count);
    uint32_t i;
    for (i = 0; i < info->num_brts; i++) {
        BRT brt = info->open_brts[i];
        int r = toku_txn_note_brt(txn, brt);
        assert(r==0);
    }
    COPY_FROM_INFO(force_fsync_on_commit );
    COPY_FROM_INFO(num_rollback_nodes);
    COPY_FROM_INFO(num_rollentries);

    CACHEFILE rollback_cachefile = txn->logger->rollback_cachefile;

    COPY_FROM_INFO(spilled_rollback_head);
    txn->spilled_rollback_head_hash = toku_cachetable_hash(rollback_cachefile,
                                                           txn->spilled_rollback_head);
    COPY_FROM_INFO(spilled_rollback_tail);
    txn->spilled_rollback_tail_hash = toku_cachetable_hash(rollback_cachefile,
                                                           txn->spilled_rollback_tail);
    COPY_FROM_INFO(current_rollback);
    txn->current_rollback_hash = toku_cachetable_hash(rollback_cachefile,
                                                      txn->current_rollback);
#undef COPY_FROM_INFO
    txn->recovered_from_checkpoint = TRUE;
    return 0;
}

// Doesn't close the txn, just performs the commit operations.
int toku_txn_commit_txn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv,
                        TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) {
    return toku_txn_commit_with_lsn(txn, nosync, yield, yieldv, ZERO_LSN,
				    poll, poll_extra);
}

struct xcommit_info {
    int r;
    TOKUTXN txn;
    int do_fsync;
};

//Called during a yield (ydb lock NOT held).
static void
local_checkpoints_and_log_xcommit(void *thunk) {
    struct xcommit_info *info = thunk;
    TOKUTXN txn = info->txn;

    if (!txn->parent && !toku_list_empty(&txn->checkpoint_before_commit)) {
        toku_poll_txn_progress_function(txn, TRUE, TRUE);
        //Do local checkpoints that must happen BEFORE logging xcommit
        uint32_t num_cachefiles = 0;
        uint32_t list_size = 16;
        CACHEFILE *cachefiles= NULL;
        XMALLOC_N(list_size, cachefiles);
        while (!toku_list_empty(&txn->checkpoint_before_commit)) {
            struct toku_list *list = toku_list_pop(&txn->checkpoint_before_commit);
            struct brt_header *h = toku_list_struct(list,
                                                    struct brt_header,
                                                    checkpoint_before_commit_link);
            cachefiles[num_cachefiles++] = h->cf;
            if (num_cachefiles == list_size) {
                list_size *= 2;
                XREALLOC_N(list_size, cachefiles);
            }
        }
        assert(num_cachefiles);
        CACHETABLE ct = toku_cachefile_get_cachetable(cachefiles[0]);

        int r = toku_cachetable_local_checkpoint_for_commit(ct, txn, num_cachefiles, cachefiles);
        assert(r==0);
        toku_free(cachefiles);
        toku_poll_txn_progress_function(txn, TRUE, FALSE);
    }

    info->r = toku_log_xcommit(txn->logger, (LSN*)0, info->do_fsync, txn->txnid64); // exits holding neither of the tokulogger locks.
}

int toku_txn_commit_with_lsn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv, LSN oplsn,
                             TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) {
    int r;
    // panic handled in log_commit

    //Child transactions do not actually 'commit'.  They promote their changes to parent, so no need to fsync if this txn has a parent.
    int do_fsync = !txn->parent && (txn->force_fsync_on_commit || (!nosync && txn->num_rollentries>0));

    txn->progress_poll_fun = poll;
    txn->progress_poll_fun_extra = poll_extra;

    {
        struct xcommit_info info = {
            .r = 0,
            .txn = txn,
            .do_fsync = do_fsync
        };
        yield(local_checkpoints_and_log_xcommit, &info, yieldv);
        r = info.r;
    }
    if (r!=0)
        return r;
    r = toku_rollback_commit(txn, yield, yieldv, oplsn);
    status.commit++;
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
    status.abort++;
    return r;
}

void toku_txn_close_txn(TOKUTXN txn) {
    toku_rollback_txn_close(txn);
    status.close++;
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
