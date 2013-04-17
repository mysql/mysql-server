/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#include "includes.h"
#include "txn.h"
#include "checkpoint.h"
#include "ule.h"
#include <valgrind/helgrind.h>
#include "txn_manager.h"

///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

static TXN_STATUS_S txn_status;

#define STATUS_INIT(k,t,l) { \
    txn_status.status[k].keyname = #k; \
    txn_status.status[k].type    = t;  \
    txn_status.status[k].legend  = "txn: " l; \
    }

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(TXN_BEGIN,            UINT64,   "begin");
    STATUS_INIT(TXN_COMMIT,           UINT64,   "successful commits");
    STATUS_INIT(TXN_ABORT,            UINT64,   "aborts");
    STATUS_INIT(TXN_CLOSE,            UINT64,   "close (should be sum of aborts and commits)");
    STATUS_INIT(TXN_NUM_OPEN,         UINT64,   "number currently open (should be begin - close)");
    STATUS_INIT(TXN_MAX_OPEN,         UINT64,   "max number open simultaneously");
    txn_status.initialized = true;
}
#undef STATUS_INIT

#define STATUS_VALUE(x) txn_status.status[x].value.num

void 
toku_txn_get_status(TXN_STATUS s) {
    if (!txn_status.initialized) {
        status_init();
    }
    *s = txn_status;
}

int 
toku_txn_begin_txn (
    DB_TXN  *container_db_txn,
    TOKUTXN parent_tokutxn, 
    TOKUTXN *tokutxn,
    TOKULOGGER logger, 
    TXN_SNAPSHOT_TYPE snapshot_type
    ) 
{
    int r = toku_txn_begin_with_xid(parent_tokutxn, tokutxn, logger, TXNID_NONE, snapshot_type, container_db_txn);
    return r;
}

int 
toku_txn_begin_with_xid (
    TOKUTXN parent_tokutxn, 
    TOKUTXN *tokutxn, 
    TOKULOGGER logger, 
    TXNID xid, 
    TXN_SNAPSHOT_TYPE snapshot_type,
    DB_TXN *container_db_txn
    ) 
{
    int r = toku_txn_create_txn(tokutxn, parent_tokutxn, logger, xid, snapshot_type, container_db_txn);
    if (r == 0) {
        toku_txn_manager_start_txn((*tokutxn)->logger->txn_manager, *tokutxn);
    }
    return r;
}

DB_TXN *
toku_txn_get_container_db_txn (TOKUTXN tokutxn) {
    DB_TXN * container = tokutxn->container_db_txn;
    return container;
}

void toku_txn_set_container_db_txn (TOKUTXN tokutxn, DB_TXN*container) {
    tokutxn->container_db_txn = container;
}

static void invalidate_xa_xid (TOKU_XA_XID *xid) {
    ANNOTATE_NEW_MEMORY(xid, sizeof(*xid)); // consider it to be all invalid for valgrind
    xid->formatID = -1; // According to the XA spec, -1 means "invalid data"
}

int 
toku_txn_create_txn (
    TOKUTXN *tokutxn, 
    TOKUTXN parent_tokutxn, 
    TOKULOGGER logger, 
    TXNID xid, 
    TXN_SNAPSHOT_TYPE snapshot_type,
    DB_TXN *container_db_txn
    ) 
{
    if (logger->is_panicked) return EINVAL;
    assert(logger->rollback_cachefile);
    TOKUTXN XMALLOC(result);
    result->starttime = time(NULL);  // getting timestamp in seconds is a cheap call
    int r;
    r = toku_omt_create(&result->open_fts);
    assert_zero(r);

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
    result->snapshot_type = snapshot_type;
    result->snapshot_txnid64 = TXNID_NONE;
    result->container_db_txn = container_db_txn;

    result->rollentry_raw_count = 0;
    result->force_fsync_on_commit = FALSE;
    result->recovered_from_checkpoint = FALSE;
    result->checkpoint_needed_before_commit = FALSE;
    result->state = TOKUTXN_LIVE;
    invalidate_xa_xid(&result->xa_xid);
    result->do_fsync = FALSE;

    result->txnid64 = xid;
    result->xids = NULL;

    *tokutxn = result;

    STATUS_VALUE(TXN_BEGIN)++;
    STATUS_VALUE(TXN_NUM_OPEN)++;
    if (STATUS_VALUE(TXN_NUM_OPEN) > STATUS_VALUE(TXN_MAX_OPEN))
        STATUS_VALUE(TXN_MAX_OPEN) = STATUS_VALUE(TXN_NUM_OPEN);

    return 0;
}

//Used on recovery to recover a transaction.
int
toku_txn_load_txninfo (TOKUTXN txn, TXNINFO info) {
#define COPY_FROM_INFO(field) txn->field = info->field
    COPY_FROM_INFO(rollentry_raw_count);
    uint32_t i;
    for (i = 0; i < info->num_fts; i++) {
        FT h = info->open_fts[i];
        int r = toku_txn_note_ft(txn, h);
        assert_zero(r);
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

int toku_txn_commit_txn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv,
                        TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra)
// Effect: Doesn't close the txn, just performs the commit operations.
//  If release_multi_operation_client_lock is true, then unlock that lock (even if an error path is taken)
{
    return toku_txn_commit_with_lsn(txn, nosync, yield, yieldv, ZERO_LSN,
                                    poll, poll_extra);
}


void
toku_txn_require_checkpoint_on_commit(TOKUTXN txn) {
    txn->checkpoint_needed_before_commit = TRUE;
}

struct xcommit_info {
    int r;
    TOKUTXN txn;
};

BOOL toku_txn_requires_checkpoint(TOKUTXN txn) {
    return (!txn->parent && txn->checkpoint_needed_before_commit);
}

//Called during a yield (ydb lock NOT held).
static void
log_xcommit(void *thunk) {
    struct xcommit_info *info = thunk;
    TOKUTXN txn = info->txn;
    info->r = toku_log_xcommit(txn->logger, &txn->do_fsync_lsn, 0, txn->txnid64); // exits holding neither of the tokulogger locks.
}

int toku_txn_commit_with_lsn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv, LSN oplsn,
                             TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) 
// Effect: Among other things: if release_multi_operation_client_lock is true, then unlock that lock (even if an error path is taken)
{
    toku_txn_manager_note_commit_txn(txn->logger->txn_manager, txn);
    int r;
    // panic handled in log_commit

    // Child transactions do not actually 'commit'.  They promote their 
    // changes to parent, so no need to fsync if this txn has a parent. The
    // do_sync state is captured in the txn for txn_maybe_fsync_log function
    // Additionally, if the transaction was first prepared, we do not need to 
    // fsync because the prepare caused an fsync of the log. In this case, 
    // we do not need an additional of the log. We rely on the client running 
    // recovery to properly recommit this transaction if the commit 
    // does not make it to disk. In the case of MySQL, that would be the
    // binary log.
    txn->do_fsync = !txn->parent && (txn->force_fsync_on_commit || (!nosync && txn->num_rollentries>0));

    txn->progress_poll_fun = poll;
    txn->progress_poll_fun_extra = poll_extra;

    {
        struct xcommit_info info = {
            .r = 0,
            .txn = txn,
        };
        log_xcommit(&info);
        r = info.r;
    }
    if (r==0) {
        r = toku_rollback_commit(txn, yield, yieldv, oplsn);
        STATUS_VALUE(TXN_COMMIT)++;
    }
    return r;
}

int toku_txn_abort_txn(TOKUTXN txn, YIELDF yield, void *yieldv,
                       TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra)
// Effect: Doesn't close the txn, just performs the abort operations.
// If release_multi_operation_client_lock is true, then unlock that lock (even if an error path is taken)
{
    return toku_txn_abort_with_lsn(txn, yield, yieldv, ZERO_LSN, poll, poll_extra);
}

int toku_txn_abort_with_lsn(TOKUTXN txn, YIELDF yield, void *yieldv, LSN oplsn,
                            TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra)
// Effect: Ammong other things, if release_multi_operation_client_lock is true, then unlock that lock (even if an error path is taken)
{
    toku_txn_manager_note_abort_txn(txn->logger->txn_manager, txn);

    txn->progress_poll_fun = poll;
    txn->progress_poll_fun_extra = poll_extra;
    int r = 0;
    txn->do_fsync = FALSE;
    r = toku_log_xabort(txn->logger, &txn->do_fsync_lsn, 0, txn->txnid64);
    if (r==0)  {
        r = toku_rollback_abort(txn, yield, yieldv, oplsn);
        STATUS_VALUE(TXN_ABORT)++;
    }
    return r;
}

static void copy_xid (TOKU_XA_XID *dest, TOKU_XA_XID *source) {
    ANNOTATE_NEW_MEMORY(dest, sizeof(*dest));
    dest->formatID     = source->formatID;
    dest->gtrid_length = source->gtrid_length;
    dest->bqual_length = source->bqual_length;
    memcpy(dest->data, source->data, source->gtrid_length+source->bqual_length);
}

int toku_txn_prepare_txn (TOKUTXN txn, TOKU_XA_XID *xa_xid) {
    if (txn->parent) return 0; // nothing to do if there's a parent.
    toku_txn_manager_add_prepared_txn(txn->logger->txn_manager, txn);
    // Do we need to do an fsync?
    txn->do_fsync = (txn->force_fsync_on_commit || txn->num_rollentries>0);
    copy_xid(&txn->xa_xid, xa_xid);
    // This list will go away with #4683, so we wn't need the ydb lock for this anymore.
    return toku_log_xprepare(txn->logger, &txn->do_fsync_lsn, 0, txn->txnid64, xa_xid);
}

void toku_txn_get_prepared_xa_xid (TOKUTXN txn, TOKU_XA_XID *xid) {
    copy_xid(xid, &txn->xa_xid);
}

int toku_logger_recover_txn (TOKULOGGER logger, struct tokulogger_preplist preplist[/*count*/], long count, /*out*/ long *retp, u_int32_t flags) {
    return toku_txn_manager_recover_txn(
        logger->txn_manager,
        preplist,
        count,
        retp,
        flags
        );
}

struct txn_fsync_log_info {
    TOKULOGGER logger;
    LSN do_fsync_lsn;
    int r;
};

static void do_txn_fsync_log(void *thunk) {
    struct txn_fsync_log_info *info = (struct txn_fsync_log_info *) thunk;
    info->r = toku_logger_fsync_if_lsn_not_fsynced(info->logger, info->do_fsync_lsn);
}

int toku_txn_maybe_fsync_log(TOKULOGGER logger, LSN do_fsync_lsn, BOOL do_fsync, YIELDF yield, void *yieldv) {
    int r = 0;
    if (logger && do_fsync) {
        struct txn_fsync_log_info info = { .logger = logger, .do_fsync_lsn = do_fsync_lsn };
        yield(do_txn_fsync_log, &info, yieldv);
        r = info.r;
    }
    return r;
}

void toku_txn_get_fsync_info(TOKUTXN ttxn, BOOL* do_fsync, LSN* do_fsync_lsn) {
    *do_fsync = ttxn->do_fsync;
    *do_fsync_lsn = ttxn->do_fsync_lsn;
}

void toku_txn_close_txn(TOKUTXN txn) {
    toku_txn_complete_txn(txn);
    toku_txn_destroy_txn(txn);
}

static int remove_txn (OMTVALUE hv, u_int32_t UU(idx), void *txnv)
// Effect:  This function is called on every open BRT that a transaction used.
//  This function removes the transaction from that BRT.
{
    FT h = hv;
    TOKUTXN txn = txnv;

    if (txn->txnid64==h->txnid_that_created_or_locked_when_empty) {
        h->txnid_that_created_or_locked_when_empty = TXNID_NONE;
    }
    if (txn->txnid64==h->txnid_that_suppressed_recovery_logs) {
        h->txnid_that_suppressed_recovery_logs = TXNID_NONE;
    }
    toku_ft_remove_txn_ref(h, txn);

    return 0;
}

// for every BRT in txn, remove it.
static void note_txn_closing (TOKUTXN txn) {
    toku_omt_iterate(txn->open_fts, remove_txn, txn);
}

void toku_txn_complete_txn(TOKUTXN txn) {
    assert(txn->spilled_rollback_head.b == ROLLBACK_NONE.b);
    assert(txn->spilled_rollback_tail.b == ROLLBACK_NONE.b);
    assert(txn->current_rollback.b == ROLLBACK_NONE.b);
    toku_txn_manager_finish_txn(txn->logger->txn_manager, txn);
    // note that here is another place we depend on
    // this function being called with the multi operation lock
    note_txn_closing(txn);
}

void toku_txn_destroy_txn(TOKUTXN txn) {
    if (txn->open_fts) {
        toku_omt_destroy(&txn->open_fts);
    }
    xids_destroy(&txn->xids);
    toku_free(txn);

    STATUS_VALUE(TXN_CLOSE)++;
    STATUS_VALUE(TXN_NUM_OPEN)--;
}

XIDS toku_txn_get_xids (TOKUTXN txn) {
    if (txn==0) return xids_get_root_xids();
    else return txn->xids;
}

void toku_txn_force_fsync_on_commit(TOKUTXN txn) {
    txn->force_fsync_on_commit = TRUE;
}

TXNID toku_get_oldest_in_live_root_txn_list(TOKUTXN txn) {
    OMT omt = txn->live_root_txn_list;
    invariant(toku_omt_size(omt)>0);
    OMTVALUE v;
    int r;
    r = toku_omt_fetch(omt, 0, &v);
    assert_zero(r);
    TXNID xid = (TXNID)v;
    return xid;
}

BOOL toku_is_txn_in_live_root_txn_list(OMT live_root_txn_list, TXNID xid) {
    OMTVALUE txnidpv;
    uint32_t index;
    BOOL retval = FALSE;
    int r = toku_omt_find_zero(live_root_txn_list, toku_find_xid_by_xid, (void *)xid, &txnidpv, &index);
    if (r==0) {
        TXNID txnid = (TXNID)txnidpv;
        invariant(txnid == xid);
        retval = TRUE;
    }
    else {
        invariant(r==DB_NOTFOUND);
    }
    return retval;
}

TOKUTXN_STATE
toku_txn_get_state(TOKUTXN txn) {
    return txn->state;
}

#include <valgrind/helgrind.h>
void __attribute__((__constructor__)) toku_txn_status_helgrind_ignore(void);
void
toku_txn_status_helgrind_ignore(void) {
    VALGRIND_HG_DISABLE_CHECKING(&txn_status, sizeof txn_status);
}

#undef STATUS_VALUE
