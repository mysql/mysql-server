/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#include "includes.h"
#include "txn.h"
#include "checkpoint.h"
#include "ule.h"


BOOL garbage_collection_debug = FALSE;

static void verify_snapshot_system(TOKULOGGER logger);

// accountability
static TXN_STATUS_S status = {.begin  = 0, 
                              .commit = 0,
                              .abort  = 0,
                              .close  = 0};


void 
toku_txn_get_status(TXN_STATUS s) {
    *s = status;
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
    int r;
    r = toku_txn_begin_with_xid(parent_tokutxn, 
				tokutxn, logger, 
				0, 
				snapshot_type
				);
    if (r == 0) {
	// container_db_txn set here, not in helper function toku_txn_begin_with_xid()
	// because helper function is used by recovery, which does not have DB_TXN
	(*tokutxn)->container_db_txn = container_db_txn; // internal struct points to container
    }
    return r;
}

DB_TXN *
toku_txn_get_container_db_txn (TOKUTXN tokutxn) {
    DB_TXN * container = tokutxn->container_db_txn;
    return container;
}

static int
fill_xids (OMTVALUE xev, u_int32_t idx, void *varray) {
    TOKUTXN txn = xev;
    TXNID *xids = varray;
    xids[idx] = txn->txnid64;
    return 0;
}


// Create list of root transactions that were live when this txn began.
static int
setup_live_root_txn_list(TOKUTXN txn) {
    int r;
    OMT global = txn->logger->live_root_txns;
    uint32_t num = toku_omt_size(global);
    // global list must have at least one live root txn, this current one
    invariant(num > 0);
    TXNID *XMALLOC_N(num, xids);
    OMTVALUE *XMALLOC_N(num, xidsp);
    uint32_t i;
    for (i = 0; i < num; i++) {
        xidsp[i] = &xids[i];
    }
    r = toku_omt_iterate(global, fill_xids, xids);
    assert_zero(r);
    
    r = toku_omt_create_steal_sorted_array(&txn->live_root_txn_list, &xidsp, num, num);
    return r;
}

// Add this txn to the global list of txns that have their own snapshots.
// (Note, if a txn is a child that creates its own snapshot, then that child xid
// is the xid stored in the global list.) 
static int
snapshot_txnids_note_txn(TOKUTXN txn) {
    int r;
    OMT txnids = txn->logger->snapshot_txnids;
    TXNID *XMALLOC(xid);
    *xid = txn->txnid64;
    r = toku_omt_insert_at(txnids, xid, toku_omt_size(txnids));
    assert_zero(r);
    return r;
}


// If live txn is not in reverse live list, then add it.
// If live txn is in reverse live list, update it by setting second xid in pair to new txn that is being started.
static int
live_list_reverse_note_txn_start_iter(OMTVALUE live_xidv, u_int32_t UU(index), void*txnv) {
    TOKUTXN txn = txnv;
    TXNID xid   = txn->txnid64;     // xid of new txn that is being started
    TXNID *live_xid = live_xidv;    // xid on the new txn's live list
    OMTVALUE pairv;
    XID_PAIR pair;
    uint32_t idx;

    int r;
    OMT reverse = txn->logger->live_list_reverse;
    r = toku_omt_find_zero(reverse, toku_find_pair_by_xid, live_xid, &pairv, &idx, NULL);
    if (r==0) {
        pair = pairv;
        invariant(pair->xid1 == *live_xid); //sanity check
        invariant(pair->xid2 < xid);        //Must be older
        pair->xid2 = txn->txnid64;
    }
    else {
        invariant(r==DB_NOTFOUND);
        //Make new entry
        XMALLOC(pair);
        pair->xid1 = *live_xid;
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

int toku_txn_begin_with_xid (
    TOKUTXN parent_tokutxn, 
    TOKUTXN *tokutxn, 
    TOKULOGGER logger, 
    TXNID xid, 
    TXN_SNAPSHOT_TYPE snapshot_type
    ) 
{
    if (logger->is_panicked) return EINVAL;
    if (garbage_collection_debug) {
        verify_snapshot_system(logger);
    }
    assert(logger->rollback_cachefile);
    TOKUTXN MALLOC(result);
    if (result==0) 
        return errno;
    int r;
    LSN first_lsn;
    result->starttime = time(NULL);  // getting timestamp in seconds is a cheap call
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
    result->snapshot_type = snapshot_type;
    result->snapshot_txnid64 = TXNID_NONE;

    if (toku_omt_size(logger->live_txns) == 0) {
        assert(logger->oldest_living_xid == TXNID_NONE_LIVING);
        logger->oldest_living_xid = result->txnid64;
        logger->oldest_living_starttime = result->starttime;
    }
    assert(logger->oldest_living_xid <= result->txnid64);

    {
        //Add txn to list (omt) of live transactions
        //We know it is the newest one.
        r = toku_omt_insert_at(logger->live_txns, result, toku_omt_size(logger->live_txns));
        if (r!=0) goto died;

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
        if (parent_tokutxn==NULL) {
            //Add txn to list (omt) of live root txns
            r = toku_omt_insert_at(logger->live_root_txns, result, toku_omt_size(logger->live_root_txns)); //We know it is the newest one.
            if (r!=0) goto died;
            result->ancestor_txnid64 = result->txnid64;
        }
        else {
            result->ancestor_txnid64 = result->parent->ancestor_txnid64;
        }

        // setup information for snapshot reads
        if (snapshot_type != TXN_SNAPSHOT_NONE) {
            // in this case, either this is a root level transaction that needs its live list setup, or it
            // is a child transaction that specifically asked for its own snapshot
            if (parent_tokutxn==NULL || snapshot_type == TXN_SNAPSHOT_CHILD) {
                r = setup_live_root_txn_list(result);  
                assert_zero(r);
                result->snapshot_txnid64 = result->txnid64;
                r = snapshot_txnids_note_txn(result);
                assert_zero(r);
                r = live_list_reverse_note_txn_start(result);
                assert_zero(r);
            }
            // in this case, it is a child transaction that specified its snapshot to be that 
            // of the root transaction
            else if (snapshot_type == TXN_SNAPSHOT_ROOT) {
                result->live_root_txn_list = result->parent->live_root_txn_list;
                result->snapshot_txnid64 = result->parent->snapshot_txnid64;
            }
            else {
                assert(FALSE);
            }
        }
    }

    result->rollentry_raw_count = 0;
    result->force_fsync_on_commit = FALSE;
    result->recovered_from_checkpoint = FALSE;
    toku_list_init(&result->checkpoint_before_commit);
    result->state = TOKUTXN_LIVE;

    // 2954
    r = toku_txn_ignore_init(result);
    if (r != 0) goto died;

    *tokutxn = result;
    status.begin++;
    if (garbage_collection_debug) {
        verify_snapshot_system(logger);
    }
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

// Doesn't close the txn, just performs the commit operations.
int toku_txn_commit_txn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv,
                        TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) {
    return toku_txn_commit_with_lsn(txn, nosync, yield, yieldv, ZERO_LSN,
				    poll, poll_extra);
}

struct xcommit_info {
    int r;
    TOKUTXN txn;
    bool do_fsync;
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
        assert_zero(r);
        toku_free(cachefiles);
        toku_poll_txn_progress_function(txn, TRUE, FALSE);
    }

    info->r = toku_log_xcommit(txn->logger, (LSN *)0, info->do_fsync, txn->txnid64); // exits holding neither of the tokulogger locks.
}

int toku_txn_commit_with_lsn(TOKUTXN txn, int nosync, YIELDF yield, void *yieldv, LSN oplsn,
                             TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) {
    txn->state = TOKUTXN_COMMITTING;
    if (garbage_collection_debug) {
        verify_snapshot_system(txn->logger);
    }
    int r;
    // panic handled in log_commit

    // Child transactions do not actually 'commit'.  They promote their changes to parent, so no need to fsync if this txn has a parent.
    // the do_sync state is captured in the txn for txn_close_txn later
    bool do_fsync = !txn->parent && (txn->force_fsync_on_commit || (!nosync && txn->num_rollentries>0));

    txn->progress_poll_fun = poll;
    txn->progress_poll_fun_extra = poll_extra;

    {
        struct xcommit_info info = {
            .r = 0,
            .txn = txn,
            .do_fsync = do_fsync,
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
    int r=0;
    r = toku_log_xabort(txn->logger, (LSN *)0, 0, txn->txnid64);
    if (r!=0) 
        return r;
    r = toku_rollback_abort(txn, yield, yieldv, oplsn);
    status.abort++;
    return r;
}

void toku_txn_close_txn(TOKUTXN txn) {
    TOKULOGGER logger = txn->logger; // capture these for the fsync after the txn is deleted

    toku_rollback_txn_close(txn); 
    txn = NULL; // txn is no longer valid

    if (garbage_collection_debug)
        verify_snapshot_system(logger);

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

TXNID toku_get_oldest_in_live_root_txn_list(TOKUTXN txn) {
    OMT omt = txn->live_root_txn_list;
    invariant(toku_omt_size(omt)>0);
    OMTVALUE v;
    int r;
    r = toku_omt_fetch(omt, 0, &v, NULL);
    assert_zero(r);
    TXNID *xidp = v;
    return *xidp;
}

//Heaviside function to find a TXNID* by TXNID* (used to find the index)
static int
find_xidp (OMTVALUE v, void *xidv) {
    TXNID xid = *(TXNID *)v;
    TXNID xidfind = *(TXNID *)xidv;
    if (xid < xidfind) return -1;
    if (xid > xidfind) return +1;
    return 0;
}

BOOL toku_is_txn_in_live_root_txn_list(TOKUTXN txn, TXNID xid) {
    OMT omt = txn->live_root_txn_list;
    OMTVALUE txnidpv;
    uint32_t index;
    BOOL retval = FALSE;
    int r = toku_omt_find_zero(omt, find_xidp, &xid, &txnidpv, &index, NULL);
    if (r==0) {
        TXNID *txnidp = txnidpv;
        invariant(*txnidp == xid);
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
        r = toku_omt_fetch(logger->snapshot_txnids, i, &v, NULL);
        assert_zero(r);
        snapshot_txnids[i] = *(TXNID*)v;
    }
    for (i = 0; i < num_live_txns; i++) {
        OMTVALUE v;
        r = toku_omt_fetch(logger->live_txns, i, &v, NULL);
        assert_zero(r);
        live_txns[i] = v;
    }
    for (i = 0; i < num_live_list_reverse; i++) {
        OMTVALUE v;
        r = toku_omt_fetch(logger->live_list_reverse, i, &v, NULL);
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
                    r = toku_omt_fetch(snapshot_txn->live_root_txn_list, j, &v, NULL);
                    assert_zero(r);
                    live_root_txn_list[j] = *(TXNID*)v;
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
                                       &pair->xid2, &v2, &index, NULL);
                assert_zero(r);
            }
            for (j = 0; j < num_live_txns; j++) {
                TOKUTXN txn = live_txns[j];
                if (txn->snapshot_type != TXN_SNAPSHOT_NONE) {
                    BOOL expect = txn->snapshot_txnid64 >= pair->xid1 &&
                                  txn->snapshot_txnid64 <= pair->xid2;
                    BOOL found = toku_is_txn_in_live_root_txn_list(txn, pair->xid1);
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
                                       &txn->txnid64, &v2, &index, NULL);
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
int toku_txn_ignore_init(TOKUTXN txn)
{
    if ( !txn ) return EINVAL;
    TXN_IGNORE txni = &(txn->ignore_errors);

    txni->fns_allocated = 0;
    txni->filenums.num = 0;
    txni->filenums.filenums = NULL;

    return 0;
}

void toku_txn_ignore_free(TOKUTXN txn)
{
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
int toku_txn_ignore_add(TOKUTXN txn, FILENUM filenum) 
{
    if ( !txn ) return EINVAL;
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
int toku_txn_ignore_remove(TOKUTXN txn, FILENUM filenum)
{
    if ( !txn ) return EINVAL; 
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
int toku_txn_ignore_contains(TOKUTXN txn, FILENUM filenum) 
{
    if ( !txn ) return EINVAL;
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
