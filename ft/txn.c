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

BOOL garbage_collection_debug = FALSE;

static void verify_snapshot_system(TOKULOGGER logger);

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
    STATUS_INIT(TXN_OLDEST_LIVE,      UINT64,   "xid of oldest live transaction");
    STATUS_INIT(TXN_OLDEST_STARTTIME, UNIXTIME, "start time of oldest live transaction");
    txn_status.initialized = true;
}
#undef STATUS_INIT

#define STATUS_VALUE(x) txn_status.status[x].value.num

void 
toku_txn_get_status(TOKULOGGER logger, TXN_STATUS s) {
    if (!txn_status.initialized)
        status_init();
    {
        time_t oldest_starttime;
        STATUS_VALUE(TXN_OLDEST_LIVE) = toku_logger_get_oldest_living_xid(logger, &oldest_starttime);
        STATUS_VALUE(TXN_OLDEST_STARTTIME) = (uint64_t) oldest_starttime;
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
    if (r == 0)
        toku_txn_start_txn(*tokutxn);
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

// Create list of root transactions that were live when this txn began.
static int
setup_live_root_txn_list(TOKUTXN txn) {
    OMT global = txn->logger->live_root_txns;
    int r = toku_omt_clone_noptr(
        &txn->live_root_txn_list,
        global
        );
    return r;
}

// Add this txn to the global list of txns that have their own snapshots.
// (Note, if a txn is a child that creates its own snapshot, then that child xid
// is the xid stored in the global list.) 
static int
snapshot_txnids_note_txn(TOKUTXN txn) {
    int r;
    OMT txnids = txn->logger->snapshot_txnids;
    r = toku_omt_insert_at(txnids, (OMTVALUE) txn->txnid64, toku_omt_size(txnids));
    assert_zero(r);
    return r;
}

// If live txn is not in reverse live list, then add it.
// If live txn is in reverse live list, update it by setting second xid in pair to new txn that is being started.
static int
live_list_reverse_note_txn_start_iter(OMTVALUE live_xidv, u_int32_t UU(index), void*txnv) {
    TOKUTXN txn = txnv;
    TXNID xid   = txn->txnid64;     // xid of new txn that is being started
    TXNID live_xid = (TXNID)live_xidv;    // xid on the new txn's live list
    OMTVALUE pairv;
    XID_PAIR pair;
    uint32_t idx;

    int r;
    OMT reverse = txn->logger->live_list_reverse;
    r = toku_omt_find_zero(reverse, toku_find_pair_by_xid, (void *)live_xid, &pairv, &idx);
    if (r==0) {
        pair = pairv;
        invariant(pair->xid1 == live_xid); //sanity check
        invariant(pair->xid2 < xid);        //Must be older
        pair->xid2 = txn->txnid64;
    }
    else {
        invariant(r==DB_NOTFOUND);
        //Make new entry
        XMALLOC(pair);
        pair->xid1 = live_xid;
        pair->xid2 = txn->txnid64;
        r = toku_omt_insert_at(reverse, pair, idx);
        assert_zero(r);
    }
    return r;
}

// Maintain the reverse live list.  The reverse live list is a list of xid pairs.  The first xid in the pair
// is a txn that was live when some txn began, and the second xid in the pair is the newest still-live xid to 
// have that first xid in its live list.  (The first xid may be closed, it only needed to be live when the 
// second txn began.)
// When a new txn begins, we need to scan the live list of this new txn.  For each live txn, we either 
// add it to the reverse live list (if it is not already there), or update to the reverse live list so
// that this new txn is the second xid in the pair associated with the txn in the live list.
static int
live_list_reverse_note_txn_start(TOKUTXN txn) {
    int r;

    r = toku_omt_iterate(txn->live_root_txn_list, live_list_reverse_note_txn_start_iter, txn);
    assert_zero(r);
    return r;
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
    if (garbage_collection_debug) {
        verify_snapshot_system(logger);
    }
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

    toku_txn_ignore_init(result); // 2954

    result->txnid64 = xid;
    result->xids = NULL;

    *tokutxn = result;

    STATUS_VALUE(TXN_BEGIN)++;
    STATUS_VALUE(TXN_NUM_OPEN)++;
    if (STATUS_VALUE(TXN_NUM_OPEN) > STATUS_VALUE(TXN_MAX_OPEN))
        STATUS_VALUE(TXN_MAX_OPEN) = STATUS_VALUE(TXN_NUM_OPEN);

    if (garbage_collection_debug) {
        verify_snapshot_system(logger);
    }
    return 0;
}

void
toku_txn_start_txn(TOKUTXN txn) {
    TOKULOGGER logger = txn->logger;
    TOKUTXN parent = txn->parent;
    int r;
    if (txn->txnid64 == TXNID_NONE) {
        LSN first_lsn;
        r = toku_log_xbegin(logger, &first_lsn, 0, parent ? parent->txnid64 : 0);
        assert_zero(r);
        txn->txnid64 = first_lsn.lsn;
    } 
    XIDS parent_xids;
    if (parent == NULL)
        parent_xids = xids_get_root_xids();
    else
        parent_xids = parent->xids;
    r = xids_create_child(parent_xids, &txn->xids, txn->txnid64);
    assert_zero(r);

    if (toku_omt_size(logger->live_txns) == 0) {
        assert(logger->oldest_living_xid == TXNID_NONE_LIVING);
        logger->oldest_living_xid = txn->txnid64;
        logger->oldest_living_starttime = txn->starttime;
    }
    assert(logger->oldest_living_xid <= txn->txnid64);

    toku_mutex_lock(&logger->txn_list_lock);
    {
        //Add txn to list (omt) of live transactions
        //We know it is the newest one.
        r = toku_omt_insert_at(logger->live_txns, txn, toku_omt_size(logger->live_txns));
        assert_zero(r);

        //
        // maintain the data structures necessary for MVCC:
        //  1. add txn to list of live_root_txns if this is a root transaction
        //  2. if the transaction is creating a snapshot:
        //    - create a live list for the transaction
        //    - add the id to the list of snapshot ids
        //    - make the necessary modifications to the live_list_reverse
        //
        // The order of operations is important here, and must be taken
        // into account when the transaction is closed. The txn is added
        // to the live_root_txns first (if it is a root txn). This has the implication
        // that a root level snapshot transaction is in its own live list. This fact
        // is taken into account when the transaction is closed.
        //

        // add ancestor information, and maintain global live root txn list
        if (parent == NULL) {
            //Add txn to list (omt) of live root txns
            r = toku_omt_insert_at(logger->live_root_txns, (OMTVALUE) txn->txnid64, toku_omt_size(logger->live_root_txns)); //We know it is the newest one.
            assert_zero(r);
            txn->ancestor_txnid64 = txn->txnid64;
        }
        else {
            txn->ancestor_txnid64 = parent->ancestor_txnid64;
        }

        // setup information for snapshot reads
        if (txn->snapshot_type != TXN_SNAPSHOT_NONE) {
            // in this case, either this is a root level transaction that needs its live list setup, or it
            // is a child transaction that specifically asked for its own snapshot
            if (parent == NULL || txn->snapshot_type == TXN_SNAPSHOT_CHILD) {
                r = setup_live_root_txn_list(txn);  
                assert_zero(r);
                txn->snapshot_txnid64 = txn->txnid64;
                r = snapshot_txnids_note_txn(txn);
                assert_zero(r);
                r = live_list_reverse_note_txn_start(txn);
                assert_zero(r);
            }
            // in this case, it is a child transaction that specified its snapshot to be that 
            // of the root transaction
            else if (txn->snapshot_type == TXN_SNAPSHOT_ROOT) {
                txn->live_root_txn_list = parent->live_root_txn_list;
                txn->snapshot_txnid64 = parent->snapshot_txnid64;
            }
            else {
                assert(FALSE);
            }
        }
    }
    toku_mutex_unlock(&logger->txn_list_lock);
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
    if (txn->state==TOKUTXN_PREPARING) {
        invalidate_xa_xid(&txn->xa_xid);
        toku_list_remove(&txn->prepared_txns_link);
    }
    txn->state = TOKUTXN_COMMITTING;
    if (garbage_collection_debug) {
        verify_snapshot_system(txn->logger);
    }
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
    if (txn->state==TOKUTXN_PREPARING) {
        invalidate_xa_xid(&txn->xa_xid);
        toku_list_remove(&txn->prepared_txns_link);
    }
    txn->state = TOKUTXN_ABORTING;
    if (garbage_collection_debug) {
        verify_snapshot_system(txn->logger);
    }
    //printf("%s:%d aborting\n", __FILE__, __LINE__);
    // Must undo everything.  Must undo it all in reverse order.
    // Build the reverse list
    //printf("%s:%d abort\n", __FILE__, __LINE__);

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
    assert(txn->state==TOKUTXN_LIVE);
    txn->state = TOKUTXN_PREPARING; // This state transition must be protected against begin_checkpoint.  Right now it uses the ydb lock.
    if (txn->parent) return 0; // nothing to do if there's a parent.
    // Do we need to do an fsync?
    txn->do_fsync = (txn->force_fsync_on_commit || txn->num_rollentries>0);
    copy_xid(&txn->xa_xid, xa_xid);
    // This list will go away with #4683, so we wn't need the ydb lock for this anymore.
    toku_list_push(&txn->logger->prepared_txns, &txn->prepared_txns_link);
    return toku_log_xprepare(txn->logger, &txn->do_fsync_lsn, 0, txn->txnid64, xa_xid);
}

void toku_txn_get_prepared_xa_xid (TOKUTXN txn, TOKU_XA_XID *xid) {
    copy_xid(xid, &txn->xa_xid);
}

int toku_logger_get_txn_from_xid (TOKULOGGER logger, TOKU_XA_XID *xid, DB_TXN **txnp) {
    int num_live_txns = toku_omt_size(logger->live_txns);
    for (int i = 0; i < num_live_txns; i++) {
        OMTVALUE v;
        {
            int r = toku_omt_fetch(logger->live_txns, i, &v);
            assert_zero(r);
        }
        TOKUTXN txn = v;
        if (txn->xa_xid.formatID     == xid->formatID
            && txn->xa_xid.gtrid_length == xid->gtrid_length
            && txn->xa_xid.bqual_length == xid->bqual_length
            && 0==memcmp(txn->xa_xid.data, xid->data, xid->gtrid_length + xid->bqual_length)) {
            *txnp = txn->container_db_txn;
            return 0;
        }
    }
    return DB_NOTFOUND;
}

int toku_logger_recover_txn (TOKULOGGER logger, struct tokulogger_preplist preplist[/*count*/], long count, /*out*/ long *retp, u_int32_t flags) {
    if (flags==DB_FIRST) {
        // Anything in the returned list goes back on the prepared list.
        while (!toku_list_empty(&logger->prepared_and_returned_txns)) {
            struct toku_list *h = toku_list_head(&logger->prepared_and_returned_txns);
            toku_list_remove(h);
            toku_list_push(&logger->prepared_txns, h);
        }
    } else if (flags!=DB_NEXT) { 
        return EINVAL;
    }
    long i;
    for (i=0; i<count; i++) {
        if (!toku_list_empty(&logger->prepared_txns)) {
            struct toku_list *h = toku_list_head(&logger->prepared_txns);
            toku_list_remove(h);
            toku_list_push(&logger->prepared_and_returned_txns, h);
            TOKUTXN txn = toku_list_struct(h, struct tokutxn, prepared_txns_link);
            assert(txn->container_db_txn);
            preplist[i].txn = txn->container_db_txn;
            preplist[i].xid = txn->xa_xid;
        } else {
            break;
        }
    }
    *retp = i;
    return 0;
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

// For each xid on the closing txn's live list, find the corresponding entry in the reverse live list.
// There must be one.
// If the second xid in the pair is not the xid of the closing transaction, then the second xid must be newer
// than the closing txn, and there is nothing to be done (except to assert the invariant).
// If the second xid in the pair is the xid of the closing transaction, then we need to find the next oldest
// txn.  If the live_xid is in the live list of the next oldest txn, then set the next oldest txn as the 
// second xid in the pair, otherwise delete the entry from the reverse live list.
static int
live_list_reverse_note_txn_end_iter(OMTVALUE live_xidv, u_int32_t UU(index), void*txnv) {
    TOKUTXN txn = txnv;
    TXNID xid = txn->txnid64;          // xid of txn that is closing
    TXNID live_xid = (TXNID)live_xidv;       // xid on closing txn's live list
    OMTVALUE pairv;
    XID_PAIR pair;
    uint32_t idx;

    int r;
    OMT reverse = txn->logger->live_list_reverse;
    r = toku_omt_find_zero(reverse, toku_find_pair_by_xid, (void *)live_xid, &pairv, &idx);
    invariant(r==0);
    pair = pairv;
    invariant(pair->xid1 == live_xid); //sanity check
    if (pair->xid2 == xid) {
        //There is a record that needs to be either deleted or updated
        TXNID olderxid;
        OMTVALUE olderv;
        uint32_t olderidx;
        OMT snapshot = txn->logger->snapshot_txnids;
        BOOL should_delete = TRUE;
        // find the youngest txn in snapshot that is older than xid
        r = toku_omt_find(snapshot, toku_find_xid_by_xid, (OMTVALUE) xid, -1, &olderv, &olderidx);
        if (r==0) {
            //There is an older txn
            olderxid = (TXNID) olderv;
            invariant(olderxid < xid);
            if (olderxid >= live_xid) {
                //older txn is new enough, we need to update.
                pair->xid2 = olderxid;
                should_delete = FALSE;
            }
        }
        else {
            invariant(r==DB_NOTFOUND);
        }
        if (should_delete) {
            //Delete record
            toku_free(pair);
            r = toku_omt_delete_at(reverse, idx);
            invariant(r==0);
        }
    }
    else {
        invariant(pair->xid2 > xid);
    }
    return r;
}

// When txn ends, update reverse live list.  To do that, examine each txn in this (closing) txn's live list.
static int
live_list_reverse_note_txn_end(TOKUTXN txn) {
    int r;

    r = toku_omt_iterate(txn->live_root_txn_list, live_list_reverse_note_txn_end_iter, txn);
    invariant(r==0);
    return r;
}


//Heaviside function to find a TOKUTXN by TOKUTXN (used to find the index)
static int find_xid (OMTVALUE v, void *txnv) {
    TOKUTXN txn = v;
    TOKUTXN txnfind = txnv;
    if (txn->txnid64<txnfind->txnid64) return -1;
    if (txn->txnid64>txnfind->txnid64) return +1;
    return 0;
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
    int r;
    TOKULOGGER logger = txn->logger;
    toku_mutex_lock(&logger->txn_list_lock);
    {
        {
            //Remove txn from list (omt) of live transactions
            OMTVALUE txnagain;
            u_int32_t idx;
            r = toku_omt_find_zero(logger->live_txns, find_xid, txn, &txnagain, &idx);
            assert(r==0);
            assert(txn==txnagain);
            r = toku_omt_delete_at(logger->live_txns, idx);
            assert(r==0);
        }

        if (txn->parent==NULL) {
            OMTVALUE v;
            u_int32_t idx;
            //Remove txn from list of live root txns
            r = toku_omt_find_zero(logger->live_root_txns, toku_find_xid_by_xid, (OMTVALUE)txn->txnid64, &v, &idx);
            assert(r==0);
            TXNID xid = (TXNID) v;
            invariant(xid == txn->txnid64);
            r = toku_omt_delete_at(logger->live_root_txns, idx);
            assert(r==0);
        }
        //
        // if this txn created a snapshot, make necessary modifications to list of snapshot txnids and live_list_reverse
        // the order of operations is important. We first remove the txnid from the list of snapshot txnids. This is
        // necessary because root snapshot transactions are in their own live lists. If we do not remove 
        // the txnid from the snapshot txnid list first, then when we go to make the modifications to 
        // live_list_reverse, we have trouble. We end up never removing (id, id) from live_list_reverse
        //
        if (txn->snapshot_type != TXN_SNAPSHOT_NONE && (txn->parent==NULL || txn->snapshot_type == TXN_SNAPSHOT_CHILD)) {
            {
                u_int32_t idx;
                OMTVALUE v;
                //Free memory used for snapshot_txnids
                r = toku_omt_find_zero(logger->snapshot_txnids, toku_find_xid_by_xid, (OMTVALUE) txn->txnid64, &v, &idx);
                invariant(r==0);
                TXNID xid = (TXNID) v;
                invariant(xid == txn->txnid64);
                r = toku_omt_delete_at(logger->snapshot_txnids, idx);
                invariant(r==0);
            }
            live_list_reverse_note_txn_end(txn);
            {
                //Free memory used for live root txns local list
                invariant(toku_omt_size(txn->live_root_txn_list) > 0);
                toku_omt_destroy(&txn->live_root_txn_list);
            }
        }
    }
    toku_mutex_unlock(&logger->txn_list_lock);

    assert(logger->oldest_living_xid <= txn->txnid64);
    if (txn->txnid64 == logger->oldest_living_xid) {
        OMTVALUE oldest_txnv;
        r = toku_omt_fetch(logger->live_txns, 0, &oldest_txnv);
        if (r==0) {
            TOKUTXN oldest_txn = oldest_txnv;
            assert(oldest_txn != txn); // We just removed it
            assert(oldest_txn->txnid64 > logger->oldest_living_xid); //Must be newer than the previous oldest
            logger->oldest_living_xid = oldest_txn->txnid64;
            logger->oldest_living_starttime = oldest_txn->starttime;
        }
        else {
            //No living transactions
            assert(r==EINVAL);
            logger->oldest_living_xid = TXNID_NONE_LIVING;
            logger->oldest_living_starttime = 0;
        }
    }

    note_txn_closing(txn);
}

void toku_txn_destroy_txn(TOKUTXN txn) {
    if (garbage_collection_debug)
        verify_snapshot_system(txn->logger);

    if (txn->open_fts)
        toku_omt_destroy(&txn->open_fts);
    xids_destroy(&txn->xids);
    toku_txn_ignore_free(txn); // 2954
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

static void
verify_snapshot_system(TOKULOGGER logger) {
    int     num_snapshot_txnids = toku_omt_size(logger->snapshot_txnids);
    TXNID       snapshot_txnids[num_snapshot_txnids];
    int     num_live_txns = toku_omt_size(logger->live_txns);
    TOKUTXN     live_txns[num_live_txns];
    int     num_live_list_reverse = toku_omt_size(logger->live_list_reverse);
    XID_PAIR    live_list_reverse[num_live_list_reverse];

    int r;
    int i;
    int j;
    //set up arrays for easier access
    for (i = 0; i < num_snapshot_txnids; i++) {
        OMTVALUE v;
        r = toku_omt_fetch(logger->snapshot_txnids, i, &v);
        assert_zero(r);
        snapshot_txnids[i] = (TXNID) v;
    }
    for (i = 0; i < num_live_txns; i++) {
        OMTVALUE v;
        r = toku_omt_fetch(logger->live_txns, i, &v);
        assert_zero(r);
        live_txns[i] = v;
    }
    for (i = 0; i < num_live_list_reverse; i++) {
        OMTVALUE v;
        r = toku_omt_fetch(logger->live_list_reverse, i, &v);
        assert_zero(r);
        live_list_reverse[i] = v;
    }

    {
        //Verify snapshot_txnids
        for (i = 0; i < num_snapshot_txnids; i++) {
            TXNID snapshot_xid = snapshot_txnids[i];
            invariant(is_txnid_live(logger, snapshot_xid));
            TOKUTXN snapshot_txn;
            r = toku_txnid2txn(logger, snapshot_xid, &snapshot_txn);
            assert_zero(r);
            int   num_live_root_txn_list = toku_omt_size(snapshot_txn->live_root_txn_list);
            TXNID     live_root_txn_list[num_live_root_txn_list];
            {
                for (j = 0; j < num_live_root_txn_list; j++) {
                    OMTVALUE v;
                    r = toku_omt_fetch(snapshot_txn->live_root_txn_list, j, &v);
                    assert_zero(r);
                    live_root_txn_list[j] = (TXNID)v;
                }
            }
            for (j = 0; j < num_live_root_txn_list; j++) {
                TXNID live_xid = live_root_txn_list[j];
                invariant(live_xid <= snapshot_xid);
                TXNID youngest = toku_get_youngest_live_list_txnid_for(
                    live_xid, 
                    logger->live_list_reverse
                    );
                invariant(youngest!=TXNID_NONE);
                invariant(youngest>=snapshot_xid);
            }
        }
    }
    {
        //Verify live_list_reverse
        for (i = 0; i < num_live_list_reverse; i++) {
            XID_PAIR pair = live_list_reverse[i];
            invariant(pair->xid1 <= pair->xid2);

            {
                //verify pair->xid2 is in snapshot_xids
                u_int32_t index;
                OMTVALUE v2;
                r = toku_omt_find_zero(logger->snapshot_txnids,
                                       toku_find_xid_by_xid,
                                       (OMTVALUE) pair->xid2, &v2, &index);
                assert_zero(r);
            }
            for (j = 0; j < num_live_txns; j++) {
                TOKUTXN txn = live_txns[j];
                if (txn->snapshot_type != TXN_SNAPSHOT_NONE) {
                    BOOL expect = txn->snapshot_txnid64 >= pair->xid1 &&
                                  txn->snapshot_txnid64 <= pair->xid2;
                    BOOL found = toku_is_txn_in_live_root_txn_list(txn->live_root_txn_list, pair->xid1);
                    invariant((expect==FALSE) == (found==FALSE));
                }
            }
        }
    }
    {
        //Verify live_txns
        for (i = 0; i < num_live_txns; i++) {
            TOKUTXN txn = live_txns[i];

            BOOL expect = txn->snapshot_txnid64 == txn->txnid64;
            {
                //verify pair->xid2 is in snapshot_xids
                u_int32_t index;
                OMTVALUE v2;
                r = toku_omt_find_zero(logger->snapshot_txnids,
                                       toku_find_xid_by_xid,
                                       (OMTVALUE) txn->txnid64, &v2, &index);
                invariant(r==0 || r==DB_NOTFOUND);
                invariant((r==0) == (expect!=0));
            }

        }
    }
}

// routines for checking if rollback errors should be ignored because a hot index create was aborted
// 2954
// returns 
//      0 on success
//      ENOMEM if can't alloc memory
//      EINVAL if txn = NULL
//      -1 on other errors
void toku_txn_ignore_init(TOKUTXN txn) {
    assert(txn);
    TXN_IGNORE txni = &(txn->ignore_errors);
    txni->fns_allocated = 0;
    txni->filenums.num = 0;
    txni->filenums.filenums = NULL;
}

void toku_txn_ignore_free(TOKUTXN txn) {
    assert(txn);
    TXN_IGNORE txni = &(txn->ignore_errors);
    toku_free(txni->filenums.filenums);
    txni->filenums.num = 0;
    txni->filenums.filenums = NULL;
}

// returns 
//      0 on success
//      ENOMEM if can't alloc memory
//      EINVAL if txn = NULL
//      -1 on other errors
int toku_txn_ignore_add(TOKUTXN txn, FILENUM filenum) {
    assert(txn);
    // check for dups
    if ( toku_txn_ignore_contains(txn, filenum) == 0 ) return 0;
    // alloc more space if needed
    const int N = 2;
    TXN_IGNORE txni = &(txn->ignore_errors);
    if ( txni->filenums.num == txni->fns_allocated ) {
        if ( txni->fns_allocated == 0 ) {
            CALLOC_N(N, txni->filenums.filenums);
            if ( txni->filenums.filenums == NULL ) return ENOMEM;
            txni->fns_allocated = N;
        }
        else {
            XREALLOC_N(txni->fns_allocated * N, txni->filenums.filenums);
            txni->fns_allocated = txni->fns_allocated * N;
        }
    }
    txni->filenums.num++;
    txni->filenums.filenums[txni->filenums.num - 1].fileid = filenum.fileid; 

    return 0;
}

// returns 
//      0 on success
//      ENOENT if not found
//      EINVAL if txn = NULL
//      -1 on other errors
// THIS FUNCTION IS NOT USED IN FUNCTIONAL CODE, BUT IS USEFUL FOR TESTING
int toku_txn_ignore_remove(TOKUTXN txn, FILENUM filenum) {
    assert(txn);
    TXN_IGNORE txni = &(txn->ignore_errors);
    int found_fn = 0;
    if ( txni->filenums.num == 0 ) return ENOENT;
    for(uint32_t i=0; i<txni->filenums.num; i++) {
        if ( !found_fn ) {
            if ( txni->filenums.filenums[i].fileid == filenum.fileid ) {
                found_fn = 1;
            }
        }
        else { // remove bubble in array
            txni->filenums.filenums[i-1].fileid = txni->filenums.filenums[i].fileid;
        }
    }
    if ( !found_fn ) return ENOENT;
    txni->filenums.num--;
    return 0;
}

// returns 
//      0 on success
//      ENOENT if not found
//      EINVAL if txn = NULL
//      -1 on other errors
int toku_txn_ignore_contains(TOKUTXN txn, FILENUM filenum) {
    assert(txn);
    TXN_IGNORE txni = &(txn->ignore_errors);
    for(uint32_t i=0; i<txni->filenums.num; i++) {
        if ( txni->filenums.filenums[i].fileid == filenum.fileid ) {
            return 0;
        }
    }
    return ENOENT;
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
