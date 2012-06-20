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

struct txn_manager {
    toku_mutex_t txn_manager_lock;  // a lock protecting this object
    OMT live_txns; // a sorted tree.  Old comment said should be a hashtable.  Do we still want that?
    OMT live_root_txns; // a sorted tree.
    OMT snapshot_txnids;    //contains TXNID x | x is snapshot txn
    // Contains 3-tuples: (TXNID begin_id, TXNID end_id, uint64_t num_live_list_references)
    //                    for committed root transaction ids that are still referenced by a live list.
    OMT referenced_xids;
    TXNID oldest_living_xid;
    time_t oldest_living_starttime;   // timestamp in seconds of when txn with oldest_living_xid started
    struct toku_list prepared_txns; // transactions that have been prepared and are unresolved, but have not been returned through txn_recover.
    struct toku_list prepared_and_returned_txns; // transactions that have been prepared and unresolved, and have been returned through txn_recover.  We need this list so that we can restart the recovery.

    toku_cond_t wait_for_unpin_of_txn;
    TXNID last_xid;
};

static TXN_MANAGER_STATUS_S txn_manager_status;
BOOL garbage_collection_debug = FALSE;


#define STATUS_INIT(k,t,l) { \
    txn_manager_status.status[k].keyname = #k; \
    txn_manager_status.status[k].type    = t;  \
    txn_manager_status.status[k].legend  = "txn: " l; \
    }

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(TXN_OLDEST_LIVE,      UINT64,   "xid of oldest live transaction");
    STATUS_INIT(TXN_OLDEST_STARTTIME, UNIXTIME, "start time of oldest live transaction");
    txn_manager_status.initialized = true;
}
#undef STATUS_INIT

#define STATUS_VALUE(x) txn_manager_status.status[x].value.num

static BOOL is_txnid_live(TXN_MANAGER txn_manager, TXNID txnid) {
    TOKUTXN result = NULL;
    toku_txn_manager_id2txn_unlocked(txn_manager, txnid, &result);
    return (result != NULL);
}

struct referenced_xid_tuple {
    TXNID begin_id;
    TXNID end_id;
    uint32_t references;
};

//Heaviside function to search through an OMT by a TXNID
static int find_by_xid (OMTVALUE v, void *txnidv);

static void
verify_snapshot_system(TXN_MANAGER txn_manager UU()) {
    uint32_t    num_snapshot_txnids = toku_omt_size(txn_manager->snapshot_txnids);
    TXNID       snapshot_txnids[num_snapshot_txnids];
    uint32_t    num_live_txns = toku_omt_size(txn_manager->live_txns);
    TOKUTXN     live_txns[num_live_txns];
    uint32_t    num_referenced_xid_tuples = toku_omt_size(txn_manager->referenced_xids);
    struct      referenced_xid_tuple  *referenced_xid_tuples[num_referenced_xid_tuples];

    int r;
    uint32_t i;
    uint32_t j;
    //set up arrays for easier access
    for (i = 0; i < num_snapshot_txnids; i++) {
        OMTVALUE v;
        r = toku_omt_fetch(txn_manager->snapshot_txnids, i, &v);
        assert_zero(r);
        snapshot_txnids[i] = (TXNID) v;
    }
    for (i = 0; i < num_live_txns; i++) {
        OMTVALUE v;
        r = toku_omt_fetch(txn_manager->live_txns, i, &v);
        assert_zero(r);
        live_txns[i] = v;
    }
    for (i = 0; i < num_referenced_xid_tuples; i++) {
        OMTVALUE v;
        r = toku_omt_fetch(txn_manager->referenced_xids, i, &v);
        assert_zero(r);
        referenced_xid_tuples[i] = v;
    }

    {
        //Verify snapshot_txnids
        for (i = 0; i < num_snapshot_txnids; i++) {
            TXNID snapshot_xid = snapshot_txnids[i];
            invariant(is_txnid_live(txn_manager, snapshot_xid));
            TOKUTXN snapshot_txn;
            toku_txn_manager_id2txn_unlocked(txn_manager, snapshot_xid, &snapshot_txn);
            uint32_t num_live_root_txn_list = toku_omt_size(snapshot_txn->live_root_txn_list);
            TXNID     live_root_txn_list[num_live_root_txn_list];
            {
                for (j = 0; j < num_live_root_txn_list; j++) {
                    OMTVALUE v;
                    r = toku_omt_fetch(snapshot_txn->live_root_txn_list, j, &v);
                    assert_zero(r);
                    live_root_txn_list[j] = (TXNID)v;
                }
            }
            {
                // Only committed entries have return a youngest.
                TXNID youngest = toku_get_youngest_live_list_txnid_for(
                    snapshot_xid,
                    txn_manager->snapshot_txnids,
                    txn_manager->referenced_xids
                    );
                invariant(youngest == TXNID_NONE);
            }
            for (j = 0; j < num_live_root_txn_list; j++) {
                TXNID live_xid = live_root_txn_list[j];
                invariant(live_xid <= snapshot_xid);
                TXNID youngest = toku_get_youngest_live_list_txnid_for(
                    live_xid,
                    txn_manager->snapshot_txnids,
                    txn_manager->referenced_xids
                    );
                if (is_txnid_live(txn_manager, live_xid)) {
                    // Only committed entries have return a youngest.
                    invariant(youngest == TXNID_NONE);
                }
                else {
                    invariant(youngest != TXNID_NONE);
                    // A committed entry might have been read-only, in which case it won't return anything.
                    // This snapshot reads 'live_xid' so it's youngest cannot be older than snapshot_xid.
                    invariant(youngest >= snapshot_xid);
                }
            }
        }
    }
    {
        // Verify referenced_xids.
        for (i = 0; i < num_referenced_xid_tuples; i++) {
            struct referenced_xid_tuple *tuple = referenced_xid_tuples[i];
            invariant(tuple->begin_id < tuple->end_id);
            invariant(tuple->references > 0);

            {
                //verify neither pair->begin_id nor end_id is in live_list
                r = toku_omt_find_zero(txn_manager->live_txns,
                                       find_by_xid,
                                       (OMTVALUE) tuple->begin_id, NULL, NULL);
                invariant(r == DB_NOTFOUND);
                r = toku_omt_find_zero(txn_manager->live_txns,
                                       find_by_xid,
                                       (OMTVALUE) tuple->end_id, NULL, NULL);
                invariant(r == DB_NOTFOUND);
            }
            {
                //verify neither pair->begin_id nor end_id is in snapshot_xids
                r = toku_omt_find_zero(txn_manager->snapshot_txnids,
                                       toku_find_xid_by_xid,
                                       (OMTVALUE) tuple->begin_id, NULL, NULL);
                invariant(r == DB_NOTFOUND);
                r = toku_omt_find_zero(txn_manager->snapshot_txnids,
                                       toku_find_xid_by_xid,
                                       (OMTVALUE) tuple->end_id, NULL, NULL);
                invariant(r == DB_NOTFOUND);
            }
            {
                // Verify number of references is correct
                uint32_t refs_found = 0;
                for (j = 0; j < num_snapshot_txnids; j++) {
                    TXNID snapshot_xid = snapshot_txnids[j];
                    TOKUTXN snapshot_txn;
                    toku_txn_manager_id2txn_unlocked(txn_manager, snapshot_xid, &snapshot_txn);

                    if (toku_is_txn_in_live_root_txn_list(snapshot_txn->live_root_txn_list, tuple->begin_id)) {
                        refs_found++;
                    }
                    invariant(!toku_is_txn_in_live_root_txn_list(
                                snapshot_txn->live_root_txn_list,
                                tuple->end_id));
                }
                invariant(refs_found == tuple->references);
            }
            {
                // Verify youngest makes sense.
                TXNID youngest = toku_get_youngest_live_list_txnid_for(
                    tuple->begin_id,
                    txn_manager->snapshot_txnids,
                    txn_manager->referenced_xids
                    );
                invariant(youngest != TXNID_NONE);
                invariant(youngest > tuple->begin_id);
                invariant(youngest < tuple->end_id);
                // Youngest must be found, and must be a snapshot txn
                r = toku_omt_find_zero(txn_manager->snapshot_txnids,
                                       toku_find_xid_by_xid,
                                       (OMTVALUE) youngest, NULL, NULL);
                invariant_zero(r);
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
                r = toku_omt_find_zero(txn_manager->snapshot_txnids,
                                       toku_find_xid_by_xid,
                                       (OMTVALUE) txn->txnid64, &v2, &index);
                invariant(r==0 || r==DB_NOTFOUND);
                invariant((r==0) == (expect!=0));
            }

        }
    }
}

static TXNID txn_manager_get_oldest_living_xid_unlocked(
    TXN_MANAGER txn_manager, 
    time_t * oldest_living_starttime
    );

void toku_txn_manager_get_status(TOKULOGGER logger, TXN_MANAGER_STATUS s) {
    if (!txn_manager_status.initialized) {
        status_init();
    }
    {
        if (logger) {
            time_t oldest_starttime;
            STATUS_VALUE(TXN_OLDEST_LIVE) = txn_manager_get_oldest_living_xid_unlocked(logger->txn_manager, &oldest_starttime);
            STATUS_VALUE(TXN_OLDEST_STARTTIME) = (uint64_t) oldest_starttime;
        }
    }
    *s = txn_manager_status;
}


void toku_txn_manager_init(TXN_MANAGER* txn_managerp) {
    int r = 0;
    TXN_MANAGER XCALLOC(txn_manager);
    toku_mutex_init(&txn_manager->txn_manager_lock, NULL);
    r = toku_omt_create(&txn_manager->live_txns);
    assert_zero(r);
    r = toku_omt_create(&txn_manager->live_root_txns);
    assert_zero(r);
    r = toku_omt_create(&txn_manager->snapshot_txnids);
    assert_zero(r);
    r = toku_omt_create(&txn_manager->referenced_xids);
    assert_zero(r);
    txn_manager->oldest_living_xid = TXNID_NONE_LIVING;
    txn_manager->oldest_living_starttime = 0;
    txn_manager->last_xid = 0;
    toku_list_init(&txn_manager->prepared_txns);
    toku_list_init(&txn_manager->prepared_and_returned_txns);

    toku_cond_init(&txn_manager->wait_for_unpin_of_txn, 0);
    *txn_managerp = txn_manager;
}

void toku_txn_manager_destroy(TXN_MANAGER txn_manager) {
    toku_mutex_destroy(&txn_manager->txn_manager_lock);
    toku_omt_destroy(&txn_manager->live_txns);
    toku_omt_destroy(&txn_manager->live_root_txns);
    toku_omt_destroy(&txn_manager->snapshot_txnids);
    toku_omt_destroy(&txn_manager->referenced_xids);
    toku_cond_destroy(&txn_manager->wait_for_unpin_of_txn);
    toku_free(txn_manager);
}

static TXNID txn_manager_get_oldest_living_xid_unlocked(
    TXN_MANAGER txn_manager,
    time_t * oldest_living_starttime
    )
{
    TXNID rval = 0;
    rval = txn_manager->oldest_living_xid;
    if (oldest_living_starttime) {
        *oldest_living_starttime = txn_manager->oldest_living_starttime;
    }
    return rval;
}

TXNID toku_txn_manager_get_oldest_living_xid(TXN_MANAGER txn_manager, time_t * oldest_living_starttime) {
    TXNID rval = 0;
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    rval = txn_manager_get_oldest_living_xid_unlocked(txn_manager, oldest_living_starttime);    
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
    return rval;
}

// Create list of root transactions that were live when this txn began.
static int
setup_live_root_txn_list(TXN_MANAGER txn_manager, TOKUTXN txn) {
    OMT global = txn_manager->live_root_txns;
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
snapshot_txnids_note_txn(TXN_MANAGER txn_manager, TOKUTXN txn) {
    int r;
    OMT txnids = txn_manager->snapshot_txnids;
    r = toku_omt_insert_at(txnids, (OMTVALUE) txn->txnid64, toku_omt_size(txnids));
    assert_zero(r);
    return r;
}

static TXNID
max_xid(TXNID a, TXNID b) {
    return a < b ? b : a;
}

int toku_txn_manager_start_txn(
    TOKUTXN *txnp,
    TXN_MANAGER txn_manager,
    TOKUTXN parent,
    TOKULOGGER logger,
    TXNID xid,
    TXN_SNAPSHOT_TYPE snapshot_type,
    DB_TXN *container_db_txn,
    bool for_recovery)
{
    int r;

    // Do as much (safe) work as possible before serializing on the txn_manager lock.
    XIDS parent_xids;
    if (parent == NULL)
        parent_xids = xids_get_root_xids();
    else
        parent_xids = parent->xids;

    TOKUTXN txn;
    r = toku_txn_create_txn(&txn, parent, logger, snapshot_type, container_db_txn, for_recovery);
    if (r != 0) {
        // logger is panicked
        return r;
    }

    // the act of getting a transaction ID and adding the
    // txn to the proper OMTs must be atomic. MVCC depends
    // on this.
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    if (garbage_collection_debug) {
        verify_snapshot_system(txn_manager);
    }
    if (xid == TXNID_NONE) {
        invariant(!for_recovery);
        xid = ++txn_manager->last_xid;
        invariant(logger);
    }
    else {
        // Recovered transactions may not come in ascending order,
        // because we assign xids when transactions are created but
        // log transactions only when they first perform a write.
        invariant(for_recovery);
        txn_manager->last_xid = max_xid(txn_manager->last_xid, xid);
    }
    XIDS xids;
    r = xids_create_child(parent_xids, &xids, xid);
    assert_zero(r);

    toku_txn_update_xids_in_txn(txn, xid, xids);

    if (toku_omt_size(txn_manager->live_txns) == 0) {
        assert(txn_manager->oldest_living_xid == TXNID_NONE_LIVING);
        txn_manager->oldest_living_xid = txn->txnid64;
        txn_manager->oldest_living_starttime = txn->starttime;
    }
    assert(txn_manager->oldest_living_xid <= txn->txnid64);

    {
        //Add txn to list (omt) of live transactions
        //We know it is the newest one.
        r = toku_omt_insert_at(txn_manager->live_txns, txn, toku_omt_size(txn_manager->live_txns));
        assert_zero(r);

        //
        // maintain the data structures necessary for MVCC:
        //  1. add txn to list of live_root_txns if this is a root transaction
        //  2. if the transaction is creating a snapshot:
        //    - create a live list for the transaction
        //    - add the id to the list of snapshot ids
        //
        // The order of operations is important here, and must be taken
        // into account when the transaction is closed. The txn is added
        // to the live_root_txns first (if it is a root txn). This has the implication
        // that a root level snapshot transaction is in its own live list. This fact
        // is taken into account when the transaction is closed.

        // add ancestor information, and maintain global live root txn list
        if (parent == NULL) {
            //Add txn to list (omt) of live root txns
            r = toku_omt_insert_at(
                txn_manager->live_root_txns,
                (OMTVALUE) txn->txnid64,
                toku_omt_size(txn_manager->live_root_txns)
                ); //We know it is the newest one.
            assert_zero(r);
        }

        // setup information for snapshot reads
        if (txn->snapshot_type != TXN_SNAPSHOT_NONE) {
            // in this case, either this is a root level transaction that needs its live list setup, or it
            // is a child transaction that specifically asked for its own snapshot
            if (parent == NULL || txn->snapshot_type == TXN_SNAPSHOT_CHILD) {
                r = setup_live_root_txn_list(txn_manager, txn);  
                assert_zero(r);
                r = snapshot_txnids_note_txn(txn_manager, txn);
                assert_zero(r);
            }
            // in this case, it is a child transaction that specified its snapshot to be that 
            // of the root transaction
            else if (txn->snapshot_type == TXN_SNAPSHOT_ROOT) {
                txn->live_root_txn_list = parent->live_root_txn_list;
            }
            else {
                assert(FALSE);
            }
        }
    }
    if (garbage_collection_debug) {
        verify_snapshot_system(txn_manager);
    }
    toku_mutex_unlock(&txn_manager->txn_manager_lock);

    *txnp = txn;
    return 0;
}

static int
find_tuple_by_xid (OMTVALUE v, void *xidv) {
    struct referenced_xid_tuple *tuple = v;
    TXNID xidfind = (TXNID)xidv;
    if (tuple->begin_id < xidfind) return -1;
    if (tuple->begin_id > xidfind) return +1;
    return 0;
}

TXNID
toku_get_youngest_live_list_txnid_for(TXNID xc, OMT snapshot_txnids, OMT referenced_xids) {
    OMTVALUE tuplev;
    struct referenced_xid_tuple *tuple;
    int r;
    TXNID rval = TXNID_NONE;

    r = toku_omt_find_zero(referenced_xids, find_tuple_by_xid, (OMTVALUE)xc, &tuplev, NULL);
    if (r == DB_NOTFOUND) {
        goto done;
    }
    tuple = tuplev;
    TXNID endid = tuple->end_id;
    TXNID live;
    OMTVALUE livev;

    r = toku_omt_find(snapshot_txnids, toku_find_xid_by_xid, (OMTVALUE)endid, -1, &livev, NULL);
    if (r == DB_NOTFOUND) {
        goto done;
    }
    live = (TXNID)livev;
    invariant(live < tuple->end_id);
    if (live > tuple->begin_id) {
        rval = live;
    }
done:
    return rval;
}

static int
referenced_xids_note_snapshot_txn_end_iter(OMTVALUE live_xidv, u_int32_t UU(index), void *referenced_xidsv) {
    OMT referenced_xids = referenced_xidsv;
    TXNID live_xid = (TXNID)live_xidv;  // xid on closing txn's live list
    int r;
    uint32_t idx;
    struct referenced_xid_tuple *tuple;
    OMTVALUE tuplev;

    r = toku_omt_find_zero(
        referenced_xids,
        find_tuple_by_xid,
        (OMTVALUE)live_xid,
        &tuplev,
        &idx);
    if (r == DB_NOTFOUND) {
        goto done;
    }
    invariant_zero(r);
    invariant(tuplev != NULL);
    tuple = tuplev;
    invariant(tuple->references > 0);
    if (--tuple->references == 0) {
        r = toku_omt_delete_at(referenced_xids, idx);
        lazy_assert_zero(r);
    }
done:
    return 0;
}

// When txn ends, update reverse live list.  To do that, examine each txn in this (closing) txn's live list.
static int
referenced_xids_note_snapshot_txn_end(TXN_MANAGER mgr, OMT live_root_txn_list) {
    int r;

    r = toku_omt_iterate(
        live_root_txn_list,
        referenced_xids_note_snapshot_txn_end_iter,
        mgr->referenced_xids);

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

void toku_txn_manager_finish_txn(TXN_MANAGER txn_manager, TOKUTXN txn) {
    int r;
    toku_mutex_lock(&txn_manager->txn_manager_lock);

    if (garbage_collection_debug) {
        verify_snapshot_system(txn_manager);
    }
    {
        //Remove txn from list (omt) of live transactions
        OMTVALUE txnagain;
        u_int32_t idx;
        r = toku_omt_find_zero(txn_manager->live_txns, find_xid, txn, &txnagain, &idx);
        invariant_zero(r);
        invariant(txn==txnagain);
        r = toku_omt_delete_at(txn_manager->live_txns, idx);
        invariant_zero(r);
    }

    bool is_snapshot = (txn->snapshot_type != TXN_SNAPSHOT_NONE && (txn->parent==NULL || txn->snapshot_type == TXN_SNAPSHOT_CHILD));
    uint32_t index_in_snapshot_txnids;
    if (is_snapshot) {
        OMTVALUE v;
        //Free memory used for snapshot_txnids
        r = toku_omt_find_zero(txn_manager->snapshot_txnids, toku_find_xid_by_xid, (OMTVALUE) txn->txnid64, &v, &index_in_snapshot_txnids);
        invariant_zero(r);
        TXNID xid = (TXNID) v;
        invariant(xid == txn->txnid64);
        r = toku_omt_delete_at(txn_manager->snapshot_txnids, index_in_snapshot_txnids);
        invariant_zero(r);

        referenced_xids_note_snapshot_txn_end(txn_manager, txn->live_root_txn_list);

        //Free memory used for live root txns local list
        invariant(toku_omt_size(txn->live_root_txn_list) > 0);
        toku_omt_destroy(&txn->live_root_txn_list);
    }

    if (txn->parent==NULL) {
        OMTVALUE v;
        u_int32_t idx;
        //Remove txn from list of live root txns
        r = toku_omt_find_zero(txn_manager->live_root_txns, toku_find_xid_by_xid, (OMTVALUE)txn->txnid64, &v, &idx);
        invariant_zero(r);
        TXNID xid = (TXNID) v;
        invariant(xid == txn->txnid64);
        r = toku_omt_delete_at(txn_manager->live_root_txns, idx);
        invariant_zero(r);

        if (txn->begin_was_logged || garbage_collection_debug) {
            if (!is_snapshot) {
                // If it's a snapshot, we already calculated index_in_snapshot_txnids.
                // Otherwise, calculate it now.
                r = toku_omt_find_zero(txn_manager->snapshot_txnids, toku_find_xid_by_xid, (OMTVALUE) txn->txnid64, NULL, &index_in_snapshot_txnids);
                invariant(r == DB_NOTFOUND);
            }
            uint32_t num_references = toku_omt_size(txn_manager->snapshot_txnids) - index_in_snapshot_txnids;
            if (num_references > 0) {
                // This transaction exists in a live list of another transaction.
                struct referenced_xid_tuple *XMALLOC(tuple);
                tuple->begin_id = txn->txnid64;
                tuple->end_id = ++txn_manager->last_xid;
                tuple->references = num_references;

                r = toku_omt_insert(
                    txn_manager->referenced_xids,
                    tuple,
                    find_tuple_by_xid,
                    (OMTVALUE)txn->txnid64,
                    NULL);
                lazy_assert_zero(r);
            }
        }
    }

    assert(txn_manager->oldest_living_xid <= txn->txnid64);
    if (txn->txnid64 == txn_manager->oldest_living_xid) {
        OMTVALUE oldest_txnv;
        r = toku_omt_fetch(txn_manager->live_txns, 0, &oldest_txnv);
        if (r==0) {
            TOKUTXN oldest_txn = oldest_txnv;
            assert(oldest_txn != txn); // We just removed it
            assert(oldest_txn->txnid64 > txn_manager->oldest_living_xid); //Must be newer than the previous oldest
            txn_manager->oldest_living_xid = oldest_txn->txnid64;
            txn_manager->oldest_living_starttime = oldest_txn->starttime;
        }
        else {
            //No living transactions
            assert(r==EINVAL);
            txn_manager->oldest_living_xid = TXNID_NONE_LIVING;
            txn_manager->oldest_living_starttime = 0;
        }
    }
    if (garbage_collection_debug) {
        verify_snapshot_system(txn_manager);
    }
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
}

void toku_txn_manager_clone_state_for_gc(
    TXN_MANAGER txn_manager,
    OMT* snapshot_xids,
    OMT* referenced_xids,
    OMT* live_root_txns
    )
{
    int r = 0;
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    r = toku_omt_clone_noptr(snapshot_xids,
                             txn_manager->snapshot_txnids);
    assert_zero(r);
    r = toku_omt_clone_pool(referenced_xids,
                            txn_manager->referenced_xids,
                            sizeof(struct referenced_xid_tuple));
    assert_zero(r);
    r = toku_omt_clone_noptr(live_root_txns,
                             txn_manager->live_root_txns);
    assert_zero(r);
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
}

//Heaviside function to search through an OMT by a TXNID
static int
find_by_xid (OMTVALUE v, void *txnidv) {
    TOKUTXN txn = v;
    TXNID   txnidfind = (TXNID)txnidv;
    if (txn->txnid64<txnidfind) return -1;
    if (txn->txnid64>txnidfind) return +1;
    return 0;
}

void toku_txn_manager_id2txn_unlocked(TXN_MANAGER txn_manager, TXNID txnid, TOKUTXN *result) {
    OMTVALUE txnfound;
    int r = toku_omt_find_zero(txn_manager->live_txns, find_by_xid, (OMTVALUE)txnid, &txnfound, NULL);
    if (r==0) {
        TOKUTXN txn = txnfound;
        assert(txn->txnid64==txnid);
        *result = txn;
    }
    else {
        assert(r==DB_NOTFOUND);
        // If there is no txn, then we treat it as the null txn.
        *result = NULL;
    }
}

void toku_txn_manager_id2txn(TXN_MANAGER txn_manager, TXNID txnid, TOKUTXN *result) {
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    toku_txn_manager_id2txn_unlocked(txn_manager, txnid, result);
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
}

int toku_txn_manager_get_txn_from_xid (TXN_MANAGER txn_manager, TOKU_XA_XID *xid, DB_TXN **txnp) {
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    int ret_val = 0;
    int num_live_txns = toku_omt_size(txn_manager->live_txns);
    for (int i = 0; i < num_live_txns; i++) {
        OMTVALUE v;
        {
            int r = toku_omt_fetch(txn_manager->live_txns, i, &v);
            assert_zero(r);
        }
        TOKUTXN txn = v;
        if (txn->xa_xid.formatID     == xid->formatID
            && txn->xa_xid.gtrid_length == xid->gtrid_length
            && txn->xa_xid.bqual_length == xid->bqual_length
            && 0==memcmp(txn->xa_xid.data, xid->data, xid->gtrid_length + xid->bqual_length)) {
            *txnp = txn->container_db_txn;
            ret_val = 0;
            goto exit;
        }
    }
    ret_val = DB_NOTFOUND;
exit:
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
    return ret_val;
}

u_int32_t toku_txn_manager_num_live_txns(TXN_MANAGER txn_manager) {
    int ret_val = 0;
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    ret_val = toku_omt_size(txn_manager->live_txns);
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
    return ret_val;
}

int toku_txn_manager_iter_over_live_txns(
    TXN_MANAGER txn_manager, 
    int (*f)(OMTVALUE, u_int32_t, void*), 
    void* v
    ) 
{
    int r = 0;
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    r = toku_omt_iterate(txn_manager->live_txns, f, v);
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
    return r;
}

void toku_txn_manager_add_prepared_txn(TXN_MANAGER txn_manager, TOKUTXN txn) {
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    assert(txn->state==TOKUTXN_LIVE);
    txn->state = TOKUTXN_PREPARING; // This state transition must be protected against begin_checkpoint
    toku_list_push(&txn_manager->prepared_txns, &txn->prepared_txns_link);
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
}

static void invalidate_xa_xid (TOKU_XA_XID *xid) {
    ANNOTATE_NEW_MEMORY(xid, sizeof(*xid)); // consider it to be all invalid for valgrind
    xid->formatID = -1; // According to the XA spec, -1 means "invalid data"
}

void toku_txn_manager_note_abort_txn(TXN_MANAGER txn_manager, TOKUTXN txn) {
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    if (txn->state==TOKUTXN_PREPARING) {
        invalidate_xa_xid(&txn->xa_xid);
        toku_list_remove(&txn->prepared_txns_link);
    }
    // for hot indexing, if hot index is processing
    // this transaction in some leafentry, then we cannot change
    // the state to commit or abort until
    // hot index is done with that leafentry
    while (txn->num_pin > 0) {
        toku_cond_wait(
            &txn_manager->wait_for_unpin_of_txn, 
            &txn_manager->txn_manager_lock
            );
    }
    txn->state = TOKUTXN_ABORTING;
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
}

void toku_txn_manager_note_commit_txn(TXN_MANAGER txn_manager, TOKUTXN txn) {
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    if (txn->state==TOKUTXN_PREPARING) {
        invalidate_xa_xid(&txn->xa_xid);
        toku_list_remove(&txn->prepared_txns_link);
    }
    // for hot indexing, if hot index is processing
    // this transaction in some leafentry, then we cannot change
    // the state to commit or abort until
    // hot index is done with that leafentry
    while (txn->num_pin > 0) {
        toku_cond_wait(
            &txn_manager->wait_for_unpin_of_txn, 
            &txn_manager->txn_manager_lock
            );
    }
    txn->state = TOKUTXN_COMMITTING;
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
}

int toku_txn_manager_recover_txn (
    TXN_MANAGER txn_manager, 
    struct tokulogger_preplist preplist[/*count*/], 
    long count, 
    long *retp, /*out*/ 
    u_int32_t flags
    )
{
    int ret_val = 0;
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    if (flags==DB_FIRST) {
        // Anything in the returned list goes back on the prepared list.
        while (!toku_list_empty(&txn_manager->prepared_and_returned_txns)) {
            struct toku_list *h = toku_list_head(&txn_manager->prepared_and_returned_txns);
            toku_list_remove(h);
            toku_list_push(&txn_manager->prepared_txns, h);
        }
    } else if (flags!=DB_NEXT) { 
        ret_val = EINVAL;
        goto exit;
    }
    long i;
    for (i=0; i<count; i++) {
        if (!toku_list_empty(&txn_manager->prepared_txns)) {
            struct toku_list *h = toku_list_head(&txn_manager->prepared_txns);
            toku_list_remove(h);
            toku_list_push(&txn_manager->prepared_and_returned_txns, h);
            TOKUTXN txn = toku_list_struct(h, struct tokutxn, prepared_txns_link);
            assert(txn->container_db_txn);
            preplist[i].txn = txn->container_db_txn;
            preplist[i].xid = txn->xa_xid;
        } else {
            break;
        }
    }
    *retp = i;
    ret_val = 0;
exit:
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
    return ret_val;
}

// needed for hot indexing

// prevents a client thread from transitioning txn from LIVE|PREPAREING -> COMMITTING|ABORTING
// hot indexing may need a transactions to stay in the LIVE|PREPARING state while it processes
// a leafentry.
void toku_txn_manager_pin_live_txn_unlocked(TXN_MANAGER UU(txn_manager), TOKUTXN txn) {
    assert(txn->state == TOKUTXN_LIVE || txn->state == TOKUTXN_PREPARING);
    txn->num_pin++;
}

// allows a client thread to go back to being able to transition txn
// from LIVE|PREPAREING -> COMMITTING|ABORTING
void toku_txn_manager_unpin_live_txn_unlocked(TXN_MANAGER txn_manager, TOKUTXN txn) {
    assert(txn->state == TOKUTXN_LIVE || txn->state == TOKUTXN_PREPARING);
    assert(txn->num_pin > 0);
    txn->num_pin--;
    if (txn->num_pin == 0) {
        toku_cond_broadcast(&txn_manager->wait_for_unpin_of_txn);
    }
}

void toku_txn_manager_suspend(TXN_MANAGER txn_manager) {
    toku_mutex_lock(&txn_manager->txn_manager_lock);
}
void toku_txn_manager_resume(TXN_MANAGER txn_manager) {
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
}

void
toku_txn_manager_set_last_xid_from_logger(TXN_MANAGER txn_manager, TOKULOGGER logger) {
    invariant(txn_manager->last_xid == TXNID_NONE);
    LSN last_lsn = toku_logger_last_lsn(logger);
    txn_manager->last_xid = last_lsn.lsn;
}

void
toku_txn_manager_set_last_xid_from_recovered_checkpoint(TXN_MANAGER txn_manager, TXNID last_xid) {
    txn_manager->last_xid = last_xid;
}

TXNID
toku_txn_manager_get_last_xid(TXN_MANAGER mgr) {
    toku_mutex_lock(&mgr->txn_manager_lock);
    TXNID last_xid = mgr->last_xid;
    toku_mutex_unlock(&mgr->txn_manager_lock);
    return last_xid;
}

// Test-only function
void
toku_txn_manager_increase_last_xid(TXN_MANAGER mgr, uint64_t increment) {
    toku_mutex_lock(&mgr->txn_manager_lock);
    mgr->last_xid += increment;
    toku_mutex_unlock(&mgr->txn_manager_lock);
}

#undef STATUS_VALUE
