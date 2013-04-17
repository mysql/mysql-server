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
#include "rollback-apply.h"
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

void
toku_txn_lock(TOKUTXN txn)
{
    toku_mutex_lock(&txn->txn_lock);
}

void
toku_txn_unlock(TOKUTXN txn)
{
    toku_mutex_unlock(&txn->txn_lock);
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
    int r = toku_txn_begin_with_xid(parent_tokutxn, tokutxn, logger, TXNID_NONE, snapshot_type, container_db_txn, false);
    return r;
}

int 
toku_txn_begin_with_xid (
    TOKUTXN parent_tokutxn, 
    TOKUTXN *tokutxn, 
    TOKULOGGER logger, 
    TXNID xid, 
    TXN_SNAPSHOT_TYPE snapshot_type,
    DB_TXN *container_db_txn,
    bool for_recovery
    ) 
{
    return toku_txn_manager_start_txn(tokutxn, logger->txn_manager, parent_tokutxn, logger, xid, snapshot_type, container_db_txn, for_recovery);
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
    XIDS xids,
    DB_TXN *container_db_txn,
    bool for_checkpoint
    ) 
{
    if (logger->is_panicked) {
        return EINVAL;
    }
    assert(logger->rollback_cachefile);

    TXNID snapshot_txnid64;
    if (snapshot_type == TXN_SNAPSHOT_NONE) {
        snapshot_txnid64 = TXNID_NONE;
    } else if (parent_tokutxn == NULL || snapshot_type == TXN_SNAPSHOT_CHILD) {
        snapshot_txnid64 = xid;
    } else if (snapshot_type == TXN_SNAPSHOT_ROOT) {
        snapshot_txnid64 = parent_tokutxn->snapshot_txnid64;
    } else {
        assert(false);
    }

    OMT open_fts;
    {
        int r = toku_omt_create(&open_fts);
        assert_zero(r);
    }

    struct txn_roll_info roll_info = {
        .num_rollback_nodes = 0,
        .num_rollentries = 0,
        .num_rollentries_processed = 0,
        .rollentry_raw_count = 0,
        .spilled_rollback_head = ROLLBACK_NONE,
        .spilled_rollback_tail = ROLLBACK_NONE,
        .spilled_rollback_head_hash = 0,
        .spilled_rollback_tail_hash = 0,
        .current_rollback = ROLLBACK_NONE,
        .current_rollback_hash = 0,
    };

    struct tokutxn new_txn = {
        .starttime = time(NULL),
        .open_fts = open_fts,
        .logger = logger,
        .parent = parent_tokutxn,
        .progress_poll_fun = NULL,
        .progress_poll_fun_extra = NULL,
        .snapshot_type = snapshot_type,
        .snapshot_txnid64 = snapshot_txnid64,
        .container_db_txn = container_db_txn,
        .force_fsync_on_commit = FALSE,
        .recovered_from_checkpoint = for_checkpoint,
        .checkpoint_needed_before_commit = FALSE,
        .state = TOKUTXN_LIVE,
        .do_fsync = FALSE,
        .txnid64 = xid,
        .ancestor_txnid64 = (parent_tokutxn ? parent_tokutxn->ancestor_txnid64 : xid),
        .xids = xids,
        .roll_info = roll_info,
        .num_pin = 0
    };


    TOKUTXN result = toku_xmemdup(&new_txn, sizeof new_txn);
    toku_mutex_init(&result->txn_lock, NULL);
    invalidate_xa_xid(&result->xa_xid);
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
    txn->roll_info.rollentry_raw_count = info->rollentry_raw_count;
    uint32_t i;
    for (i = 0; i < info->num_fts; i++) {
        FT ft = info->open_fts[i];
        toku_txn_maybe_note_ft(txn, ft);
    }
    txn->force_fsync_on_commit = info->force_fsync_on_commit;
    txn->roll_info.num_rollback_nodes = info->num_rollback_nodes;
    txn->roll_info.num_rollentries = info->num_rollentries;

    CACHEFILE rollback_cachefile = txn->logger->rollback_cachefile;

    txn->roll_info.spilled_rollback_head = info->spilled_rollback_head;
    txn->roll_info.spilled_rollback_head_hash = toku_cachetable_hash(rollback_cachefile,
                                                           txn->roll_info.spilled_rollback_head);
    txn->roll_info.spilled_rollback_tail = info->spilled_rollback_tail;
    txn->roll_info.spilled_rollback_tail_hash = toku_cachetable_hash(rollback_cachefile,
                                                           txn->roll_info.spilled_rollback_tail);
    txn->roll_info.current_rollback = info->current_rollback;
    txn->roll_info.current_rollback_hash = toku_cachetable_hash(rollback_cachefile,
                                                      txn->roll_info.current_rollback);
    return 0;
}

int toku_txn_commit_txn(TOKUTXN txn, int nosync,
                        TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra)
// Effect: Doesn't close the txn, just performs the commit operations.
//  If release_multi_operation_client_lock is true, then unlock that lock (even if an error path is taken)
{
    return toku_txn_commit_with_lsn(txn, nosync, ZERO_LSN,
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

int toku_txn_commit_with_lsn(TOKUTXN txn, int nosync, LSN oplsn,
                             TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) 
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
    txn->do_fsync = !txn->parent && (txn->force_fsync_on_commit || (!nosync && txn->roll_info.num_rollentries>0));

    txn->progress_poll_fun = poll;
    txn->progress_poll_fun_extra = poll_extra;

    r = toku_log_xcommit(txn->logger, &txn->do_fsync_lsn, 0, txn->txnid64);
    if (r==0) {
        r = toku_rollback_commit(txn, oplsn);
        STATUS_VALUE(TXN_COMMIT)++;
    }
    return r;
}

int toku_txn_abort_txn(TOKUTXN txn,
                       TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra)
// Effect: Doesn't close the txn, just performs the abort operations.
// If release_multi_operation_client_lock is true, then unlock that lock (even if an error path is taken)
{
    return toku_txn_abort_with_lsn(txn, ZERO_LSN, poll, poll_extra);
}

int toku_txn_abort_with_lsn(TOKUTXN txn, LSN oplsn,
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
        r = toku_rollback_abort(txn, oplsn);
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
    txn->do_fsync = (txn->force_fsync_on_commit || txn->roll_info.num_rollentries>0);
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

int toku_txn_maybe_fsync_log(TOKULOGGER logger, LSN do_fsync_lsn, BOOL do_fsync) {
    int r = 0;
    if (logger && do_fsync) {
        struct txn_fsync_log_info info = { .logger = logger, .do_fsync_lsn = do_fsync_lsn };
        //TODO(yoni): inline do_txn_fsync_log here
        do_txn_fsync_log(&info);
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
    assert(txn->roll_info.spilled_rollback_head.b == ROLLBACK_NONE.b);
    assert(txn->roll_info.spilled_rollback_tail.b == ROLLBACK_NONE.b);
    assert(txn->roll_info.current_rollback.b == ROLLBACK_NONE.b);
    assert(txn->num_pin == 0);
    assert(txn->state == TOKUTXN_COMMITTING || txn->state == TOKUTXN_ABORTING);
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
    toku_mutex_destroy(&txn->txn_lock);
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
