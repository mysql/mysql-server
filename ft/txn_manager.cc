/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_race_tools.h>

#include <util/omt.h>

#include "log-internal.h"
#include "txn.h"
#include "checkpoint.h"
#include "ule.h"
#include "txn_manager.h"
#include "rollback.h"

bool garbage_collection_debug = false;

// internal locking functions, should use this instead of accessing lock directly
static void txn_manager_lock(TXN_MANAGER txn_manager);
static void txn_manager_unlock(TXN_MANAGER txn_manager);

#if 0
static bool is_txnid_live(TXN_MANAGER txn_manager, TXNID txnid) {
    TOKUTXN result = NULL;
    toku_txn_manager_id2txn_unlocked(txn_manager, txnid, &result);
    return (result != NULL);
}
#endif

//Heaviside function to search through an OMT by a TXNID
int find_by_xid (const TOKUTXN &txn, const TXNID &txnidfind);

static bool is_txnid_live(TXN_MANAGER txn_manager, TXNID txnid) {
    TOKUTXN result = NULL;
    TXNID_PAIR id = { .parent_id64 = txnid, .child_id64 = TXNID_NONE };
    toku_txn_manager_id2txn_unlocked(txn_manager, id, &result);
    return (result != NULL);
}

static void toku_txn_manager_clone_state_for_gc_unlocked(
    TXN_MANAGER txn_manager,
    xid_omt_t* snapshot_xids,
    rx_omt_t* referenced_xids,
    xid_omt_t* live_root_txns
    );

static void
verify_snapshot_system(TXN_MANAGER txn_manager UU()) {
    uint32_t    num_snapshot_txnids = txn_manager->num_snapshots;
    TXNID       snapshot_txnids[num_snapshot_txnids];
    TOKUTXN     snapshot_txns[num_snapshot_txnids];
    uint32_t    num_live_txns = txn_manager->live_root_txns.size();
    TOKUTXN     live_txns[num_live_txns];
    uint32_t    num_referenced_xid_tuples = txn_manager->referenced_xids.size();
    struct      referenced_xid_tuple  *referenced_xid_tuples[num_referenced_xid_tuples];

    // do this to get an omt of snapshot_txnids
    xid_omt_t snapshot_txnids_omt;
    rx_omt_t referenced_xids_omt;
    xid_omt_t live_root_txns_omt;
    toku_txn_manager_clone_state_for_gc_unlocked(
        txn_manager,
        &snapshot_txnids_omt,
        &referenced_xids_omt,
        &live_root_txns_omt
        );    

    int r;
    uint32_t i;
    uint32_t j;
    //set up arrays for easier access
    {
        TOKUTXN curr_txn = txn_manager->snapshot_head;
        uint32_t curr_index = 0;
        while (curr_txn != NULL) {
            snapshot_txns[curr_index] = curr_txn;
            snapshot_txnids[curr_index] = curr_txn->snapshot_txnid64;
            curr_txn = curr_txn->snapshot_next;
            curr_index++;
        }
    }

    for (i = 0; i < num_live_txns; i++) {
        r = txn_manager->live_root_txns.fetch(i, &live_txns[i]);
        assert_zero(r);
    }
    for (i = 0; i < num_referenced_xid_tuples; i++) {
        r = txn_manager->referenced_xids.fetch(i, &referenced_xid_tuples[i]);
        assert_zero(r);
    }

    {
        //Verify snapshot_txnids
        for (i = 0; i < num_snapshot_txnids; i++) {
            TXNID snapshot_xid = snapshot_txnids[i];
            TOKUTXN snapshot_txn = snapshot_txns[i];
            uint32_t num_live_root_txn_list = snapshot_txn->live_root_txn_list->size();
            TXNID     live_root_txn_list[num_live_root_txn_list];
            {
                for (j = 0; j < num_live_root_txn_list; j++) {
                    r = snapshot_txn->live_root_txn_list->fetch(j, &live_root_txn_list[j]);
                    assert_zero(r);
                }
            }
            {
                // Only committed entries have return a youngest.
                TXNID youngest = toku_get_youngest_live_list_txnid_for(
                    snapshot_xid,
                    snapshot_txnids_omt,
                    txn_manager->referenced_xids
                    );
                invariant(youngest == TXNID_NONE);
            }
            for (j = 0; j < num_live_root_txn_list; j++) {
                TXNID live_xid = live_root_txn_list[j];
                invariant(live_xid <= snapshot_xid);
                TXNID youngest = toku_get_youngest_live_list_txnid_for(
                    live_xid,
                    snapshot_txnids_omt,
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
                r = txn_manager->live_root_txns.find_zero<TXNID, find_by_xid>(tuple->begin_id, nullptr, nullptr);
                invariant(r == DB_NOTFOUND);
                r = txn_manager->live_root_txns.find_zero<TXNID, find_by_xid>(tuple->end_id, nullptr, nullptr);
                invariant(r == DB_NOTFOUND);
            }
            {
                //verify neither pair->begin_id nor end_id is in snapshot_xids                
                TOKUTXN curr_txn = txn_manager->snapshot_head;
                uint32_t curr_index = 0;
                while (curr_txn != NULL) {
                    invariant(tuple->begin_id != curr_txn->txnid.parent_id64);
                    invariant(tuple->end_id != curr_txn->txnid.parent_id64);
                    curr_txn = curr_txn->snapshot_next;
                    curr_index++;
                }
            }
            {
                // Verify number of references is correct
                uint32_t refs_found = 0;
                for (j = 0; j < num_snapshot_txnids; j++) {
                    TOKUTXN snapshot_txn = snapshot_txns[j];
                    if (toku_is_txn_in_live_root_txn_list(*snapshot_txn->live_root_txn_list, tuple->begin_id)) {
                        refs_found++;
                    }
                    invariant(!toku_is_txn_in_live_root_txn_list(
                                *snapshot_txn->live_root_txn_list,
                                tuple->end_id));
                }
                invariant(refs_found == tuple->references);
            }
            {
                // Verify youngest makes sense.
                TXNID youngest = toku_get_youngest_live_list_txnid_for(
                    tuple->begin_id,
                    snapshot_txnids_omt,
                    txn_manager->referenced_xids
                    );
                invariant(youngest != TXNID_NONE);
                invariant(youngest > tuple->begin_id);
                invariant(youngest < tuple->end_id);
                // Youngest must be found, and must be a snapshot txn
                r = snapshot_txnids_omt.find_zero<TXNID, toku_find_xid_by_xid>(youngest, nullptr, nullptr);
                invariant_zero(r);
            }
        }
    }
    snapshot_txnids_omt.destroy();
    referenced_xids_omt.destroy();
    live_root_txns_omt.destroy();
}

void toku_txn_manager_init(TXN_MANAGER* txn_managerp) {
    TXN_MANAGER XCALLOC(txn_manager);
    toku_mutex_init(&txn_manager->txn_manager_lock, NULL);
    txn_manager->live_root_txns.create();
    txn_manager->live_root_ids.create();
    txn_manager->snapshot_head = NULL;
    txn_manager->snapshot_tail = NULL;
    txn_manager->num_snapshots = 0;
    txn_manager->referenced_xids.create();
    txn_manager->last_xid = 0;

    txn_manager->last_xid_seen_for_recover = TXNID_NONE;
    txn_manager->last_calculated_oldest_referenced_xid = TXNID_NONE;
    
    *txn_managerp = txn_manager;
}

void toku_txn_manager_destroy(TXN_MANAGER txn_manager) {
    toku_mutex_destroy(&txn_manager->txn_manager_lock);
    invariant(txn_manager->live_root_txns.size() == 0);
    txn_manager->live_root_txns.destroy();
    invariant(txn_manager->live_root_ids.size() == 0);
    txn_manager->live_root_ids.destroy();
    invariant(txn_manager->snapshot_head == NULL);
    invariant(txn_manager->referenced_xids.size() == 0);
    txn_manager->referenced_xids.destroy();
    toku_free(txn_manager);
}

TXNID
toku_txn_manager_get_oldest_living_xid(TXN_MANAGER txn_manager) {
    TOKUTXN rtxn = NULL;
    TXNID rval = TXNID_NONE_LIVING;
    txn_manager_lock(txn_manager);

    if (txn_manager->live_root_txns.size() > 0) {
        int r = txn_manager->live_root_txns.fetch(0, &rtxn);
        invariant_zero(r);
    }
    if (rtxn) {
        rval = rtxn->txnid.parent_id64;
    }
    txn_manager_unlock(txn_manager);
    return rval;
}

TXNID toku_txn_manager_get_oldest_referenced_xid_estimate(TXN_MANAGER txn_manager) {
    return txn_manager->last_calculated_oldest_referenced_xid;
}

int live_root_txn_list_iter(const TOKUTXN &live_xid, const uint32_t UU(index), TXNID **const referenced_xids);
int live_root_txn_list_iter(const TOKUTXN &live_xid, const uint32_t UU(index), TXNID **const referenced_xids){
    (*referenced_xids)[index] = live_xid->txnid.parent_id64;
    return 0;
}


// Create list of root transactions that were live when this txn began.
static inline void
setup_live_root_txn_list(xid_omt_t* live_root_txnid, xid_omt_t* live_root_txn_list) {
    live_root_txn_list->clone(*live_root_txnid);
}

//Heaviside function to search through an OMT by a TXNID
int
find_by_xid (const TOKUTXN &txn, const TXNID &txnidfind) {
    if (txn->txnid.parent_id64 < txnidfind) return -1;
    if (txn->txnid.parent_id64 > txnidfind) return +1;
    return 0;
}

#if 0
static void
omt_insert_at_end_unless_recovery(OMT omt, int (*h)(OMTVALUE, void*extra), TOKUTXN txn, OMTVALUE v, bool for_recovery)
// Effect: insert v into omt that is sorted by xid gotten from txn.
// Rationale:
//   During recovery, we get txns in the order that they did their first
//   write operation, which is not necessarily monotonically increasing.
//   During normal operation, txns are created with strictly increasing
//   txnids, so we can always insert at the end.
{
    int r;
    uint32_t idx = toku_omt_size(omt);
    if (for_recovery) {
        r = toku_omt_find_zero(omt, h, (void *) txn->txnid64, NULL, &idx);
        invariant(r==DB_NOTFOUND);
    }
    r = toku_omt_insert_at(omt, v, idx);
    lazy_assert_zero(r);
}
#endif

static TXNID
max_xid(TXNID a, TXNID b) {
    return a < b ? b : a;
}

static void set_oldest_referenced_xid(TXN_MANAGER txn_manager) {
    TXNID oldest_referenced_xid = TXNID_MAX;
    int r;
    if (txn_manager->live_root_ids.size() > 0) {
        r = txn_manager->live_root_ids.fetch(0, &oldest_referenced_xid);
        // this function should only be called when we know there is at least
        // one live transaction
        invariant_zero(r);
    }
    
    if (txn_manager->referenced_xids.size() > 0) {
        struct referenced_xid_tuple* tuple;
        r = txn_manager->referenced_xids.fetch(0, &tuple);
        if (r == 0 && tuple->begin_id < oldest_referenced_xid) {
            oldest_referenced_xid = tuple->begin_id;
        }
    }
    if (txn_manager->snapshot_head != NULL) {
        TXNID id = txn_manager->snapshot_head->snapshot_txnid64;
        if (id < oldest_referenced_xid) {
            oldest_referenced_xid = id;
        }
    }
    if (txn_manager->last_xid < oldest_referenced_xid) {
        oldest_referenced_xid = txn_manager->last_xid;
    }
    invariant(oldest_referenced_xid != TXNID_MAX);
    txn_manager->last_calculated_oldest_referenced_xid = oldest_referenced_xid;
}

//Heaviside function to find a TOKUTXN by TOKUTXN (used to find the index)
// template-only function, but must be extern
int find_xid (const TOKUTXN &txn, const TOKUTXN &txnfind);
int
find_xid (const TOKUTXN &txn, const TOKUTXN &txnfind)
{
    if (txn->txnid.parent_id64 < txnfind->txnid.parent_id64) return -1;
    if (txn->txnid.parent_id64 > txnfind->txnid.parent_id64) return +1;
    return 0;
}

static inline void txn_manager_create_snapshot_unlocked(
    TXN_MANAGER txn_manager,
    TOKUTXN txn
    ) 
{    
    txn->snapshot_txnid64 = ++txn_manager->last_xid;    
    setup_live_root_txn_list(&txn_manager->live_root_ids, txn->live_root_txn_list);  
    // Add this txn to the global list of txns that have their own snapshots.
    // (Note, if a txn is a child that creates its own snapshot, then that child xid
    // is the xid stored in the global list.) 
    if (txn_manager->snapshot_head == NULL) {
        invariant(txn_manager->snapshot_tail == NULL);
        txn_manager->snapshot_head = txn;
        txn_manager->snapshot_tail = txn;
    }
    else {
        txn_manager->snapshot_tail->snapshot_next = txn;
        txn->snapshot_prev = txn_manager->snapshot_tail;
        txn_manager->snapshot_tail = txn;
    }
    txn_manager->num_snapshots++;
}

// template-only function, but must be extern
int find_tuple_by_xid (const struct referenced_xid_tuple &tuple, const TXNID &xidfind);
int
find_tuple_by_xid (const struct referenced_xid_tuple &tuple, const TXNID &xidfind)
{
    if (tuple.begin_id < xidfind) return -1;
    if (tuple.begin_id > xidfind) return +1;
    return 0;
}

// template-only function, but must be extern
int referenced_xids_note_snapshot_txn_end_iter(const TXNID &live_xid, const uint32_t UU(index), rx_omt_t *const referenced_xids)
    __attribute__((nonnull(3)));
int referenced_xids_note_snapshot_txn_end_iter(const TXNID &live_xid, const uint32_t UU(index), rx_omt_t *const referenced_xids)
{
    int r;
    uint32_t idx;
    struct referenced_xid_tuple *tuple;

    r = referenced_xids->find_zero<TXNID, find_tuple_by_xid>(live_xid, &tuple, &idx);
    if (r == DB_NOTFOUND) {
        goto done;
    }
    invariant_zero(r);
    invariant(tuple->references > 0);
    if (--tuple->references == 0) {
        r = referenced_xids->delete_at(idx);
        lazy_assert_zero(r);
    }
done:
    return 0;
}

// When txn ends, update reverse live list.  To do that, examine each txn in this (closing) txn's live list.
static inline int
note_snapshot_txn_end_by_ref_xids(TXN_MANAGER mgr, const xid_omt_t &live_root_txn_list) {
    int r;
    r = live_root_txn_list.iterate<rx_omt_t, referenced_xids_note_snapshot_txn_end_iter>(&mgr->referenced_xids);
    invariant_zero(r);
    return r;
}

typedef struct snapshot_iter_extra {
    uint32_t* indexes_to_delete;
    uint32_t num_indexes;
    xid_omt_t* live_root_txn_list;
} SNAPSHOT_ITER_EXTRA;

// template-only function, but must be extern
int note_snapshot_txn_end_by_txn_live_list_iter(referenced_xid_tuple* tuple, const uint32_t index, SNAPSHOT_ITER_EXTRA *const sie)
    __attribute__((nonnull(3)));
int note_snapshot_txn_end_by_txn_live_list_iter(
    referenced_xid_tuple* tuple, 
    const uint32_t index, 
    SNAPSHOT_ITER_EXTRA *const sie
    )
{
    int r;
    uint32_t idx;
    TXNID txnid;
    r = sie->live_root_txn_list->find_zero<TXNID, toku_find_xid_by_xid>(tuple->begin_id, &txnid, &idx);
    if (r == DB_NOTFOUND) {
        goto done;
    }
    invariant_zero(r);
    invariant(txnid == tuple->begin_id);
    invariant(tuple->references > 0);
    if (--tuple->references == 0) {
        sie->indexes_to_delete[sie->num_indexes] = index;
        sie->num_indexes++;
    }
done:
    return 0;
}

static inline int
note_snapshot_txn_end_by_txn_live_list(TXN_MANAGER mgr, xid_omt_t* live_root_txn_list) {
    uint32_t size = mgr->referenced_xids.size();
    uint32_t indexes_to_delete[size];
    SNAPSHOT_ITER_EXTRA sie = { .indexes_to_delete = indexes_to_delete, .num_indexes = 0, .live_root_txn_list = live_root_txn_list};
    mgr->referenced_xids.iterate_ptr<SNAPSHOT_ITER_EXTRA, note_snapshot_txn_end_by_txn_live_list_iter>(&sie);
    for (uint32_t i = 0; i < sie.num_indexes; i++) {
        uint32_t curr_index = sie.indexes_to_delete[sie.num_indexes-i-1];
        mgr->referenced_xids.delete_at(curr_index);
    }
    return 0;
}

static inline void txn_manager_remove_snapshot_unlocked(
    TOKUTXN txn, 
    TXN_MANAGER txn_manager
    ) 
{
    // Remove from linked list of snapshot txns
    if (txn_manager->snapshot_head == txn) {
        txn_manager->snapshot_head = txn->snapshot_next;
    }
    if (txn_manager->snapshot_tail == txn) {
        txn_manager->snapshot_tail = txn->snapshot_prev;
    }
    if (txn->snapshot_next) {
        txn->snapshot_next->snapshot_prev = txn->snapshot_prev;
    }
    if (txn->snapshot_prev) {
        txn->snapshot_prev->snapshot_next = txn->snapshot_next;
    }    
    txn_manager->num_snapshots--;
    uint32_t ref_xids_size = txn_manager->referenced_xids.size();
    uint32_t live_list_size = txn->live_root_txn_list->size();
    if (ref_xids_size > 0 && live_list_size > 0) {
        if (live_list_size > ref_xids_size && ref_xids_size < 2000) {
            note_snapshot_txn_end_by_txn_live_list(txn_manager, txn->live_root_txn_list);
        }
        else {
            note_snapshot_txn_end_by_ref_xids(txn_manager, *txn->live_root_txn_list);
        }
    }
}

static inline void inherit_snapshot_from_parent(TOKUTXN child) {
    if (child->parent) {
        child->snapshot_txnid64 = child->parent->snapshot_txnid64;
        child->live_root_txn_list = child->parent->live_root_txn_list;
    }
}
void toku_txn_manager_handle_snapshot_create_for_child_txn(
    TOKUTXN txn,
    TXN_MANAGER txn_manager,
    TXN_SNAPSHOT_TYPE snapshot_type
    ) 
{
    // this is a function for child txns, so just doint a sanity check
    invariant(txn->parent != NULL);
    bool needs_snapshot = txn_needs_snapshot(snapshot_type, txn->parent);
    if (needs_snapshot) {
        invariant(txn->live_root_txn_list == nullptr);
        XMALLOC(txn->live_root_txn_list);
        txn_manager_lock(txn_manager);
        txn_manager_create_snapshot_unlocked(txn_manager, txn);
        txn_manager_unlock(txn_manager);
    }
    else {
        inherit_snapshot_from_parent(txn);
    }
}

void toku_txn_manager_handle_snapshot_destroy_for_child_txn(
    TOKUTXN txn,
    TXN_MANAGER txn_manager,
    TXN_SNAPSHOT_TYPE snapshot_type
    )
{
    // this is a function for child txns, so just doint a sanity check
    invariant(txn->parent != NULL);
    bool is_snapshot = txn_needs_snapshot(snapshot_type, txn->parent);
    if (is_snapshot) {
        txn_manager_lock(txn_manager);
        txn_manager_remove_snapshot_unlocked(txn, txn_manager);
        txn_manager_unlock(txn_manager);
        invariant(txn->live_root_txn_list != nullptr);
        txn->live_root_txn_list->destroy();
        toku_free(txn->live_root_txn_list);
    }
}

void toku_txn_manager_start_txn_for_recovery(
    TOKUTXN txn,
    TXN_MANAGER txn_manager,
    TXNID xid
    )
{
    txn_manager_lock(txn_manager);
    // using xid that is passed in
    txn_manager->last_xid = max_xid(txn_manager->last_xid, xid);
    toku_txn_update_xids_in_txn(txn, xid);

    uint32_t idx;
    int r = txn_manager->live_root_txns.find_zero<TOKUTXN, find_xid>(txn, nullptr, &idx);
    invariant(r == DB_NOTFOUND);
    r = txn_manager->live_root_txns.insert_at(txn, idx);
    invariant_zero(r);
    r = txn_manager->live_root_ids.insert_at(txn->txnid.parent_id64, idx);
    invariant_zero(r);

    txn_manager_unlock(txn_manager);
}

void toku_txn_manager_start_txn(
    TOKUTXN txn,
    TXN_MANAGER txn_manager,
    TXN_SNAPSHOT_TYPE snapshot_type,
    bool read_only
    )
{
    int r;
    TXNID xid = TXNID_NONE;
    // if we are running in recovery, we don't need to make snapshots
    bool needs_snapshot = txn_needs_snapshot(snapshot_type, NULL);

    // perform a malloc outside of the txn_manager lock
    // will be used in txn_manager_create_snapshot_unlocked below
    if (needs_snapshot) {
        invariant(txn->live_root_txn_list == nullptr);
        XMALLOC(txn->live_root_txn_list);
    }
    // the act of getting a transaction ID and adding the
    // txn to the proper OMTs must be atomic. MVCC depends
    // on this.
    txn_manager_lock(txn_manager);
    if (garbage_collection_debug) {
        verify_snapshot_system(txn_manager);
    }

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
    xid = ++txn_manager->last_xid; // we always need an ID, needed for lock tree
    toku_txn_update_xids_in_txn(txn, xid);
    if (!read_only) {
        uint32_t idx = txn_manager->live_root_txns.size();
        r = txn_manager->live_root_txns.insert_at(txn, idx);
        invariant_zero(r);
        r = txn_manager->live_root_ids.insert_at(txn->txnid.parent_id64, idx);
        invariant_zero(r);
    }
    set_oldest_referenced_xid(txn_manager);
    
    if (needs_snapshot) {
        txn_manager_create_snapshot_unlocked(
            txn_manager,
            txn
            );
    }

    if (garbage_collection_debug) {
        verify_snapshot_system(txn_manager);
    }
    txn_manager_unlock(txn_manager);
    return;
}

TXNID
toku_get_youngest_live_list_txnid_for(TXNID xc, const xid_omt_t &snapshot_txnids, const rx_omt_t &referenced_xids) {
    struct referenced_xid_tuple *tuple;
    int r;
    TXNID rval = TXNID_NONE;

    r = referenced_xids.find_zero<TXNID, find_tuple_by_xid>(xc, &tuple, nullptr);
    if (r == DB_NOTFOUND) {
        goto done;
    }
    TXNID live;

    r = snapshot_txnids.find<TXNID, toku_find_xid_by_xid>(tuple->end_id, -1, &live, nullptr);
    if (r == DB_NOTFOUND) {
        goto done;
    }
    invariant(live < tuple->end_id);
    if (live > tuple->begin_id) {
        rval = live;
    }
done:
    return rval;
}

void toku_txn_manager_finish_txn(TXN_MANAGER txn_manager, TOKUTXN txn) {
    int r;
    invariant(txn->parent == NULL);
    bool is_snapshot = txn_needs_snapshot(txn->snapshot_type, NULL);
    txn_manager_lock(txn_manager);

    if (garbage_collection_debug) {
        verify_snapshot_system(txn_manager);
    }

    if (is_snapshot) {
        txn_manager_remove_snapshot_unlocked(
            txn, 
            txn_manager
            );
    }

    if (!txn_declared_read_only(txn)) {
        uint32_t idx;
        //Remove txn from list of live root txns
        TOKUTXN txnagain;
        r = txn_manager->live_root_txns.find_zero<TOKUTXN, find_xid>(txn, &txnagain, &idx);
        invariant_zero(r);
        invariant(txn==txnagain);

        r = txn_manager->live_root_txns.delete_at(idx);
        invariant_zero(r);
        r = txn_manager->live_root_ids.delete_at(idx);
        invariant_zero(r);

        if (!toku_txn_is_read_only(txn) || garbage_collection_debug) {
            uint32_t num_references = 0;
            TOKUTXN curr_txn = txn_manager->snapshot_tail;
            while(curr_txn != NULL) {
                if (curr_txn->snapshot_txnid64 > txn->txnid.parent_id64) {
                    num_references++;
                }
                else {
                    break;
                }
                curr_txn = curr_txn->snapshot_prev;
            }
            
            if (num_references > 0) {
                // This transaction exists in a live list of another transaction.
                struct referenced_xid_tuple tuple = {
                    .begin_id = txn->txnid.parent_id64,
                    .end_id = ++txn_manager->last_xid,
                    .references = num_references
                };
                r = txn_manager->referenced_xids.insert<TXNID, find_tuple_by_xid>(tuple, txn->txnid.parent_id64, nullptr);
                lazy_assert_zero(r);
            }
        }
    }

    if (garbage_collection_debug) {
        verify_snapshot_system(txn_manager);
    }
    txn_manager_unlock(txn_manager);

    //Cleanup that does not require the txn_manager lock
    if (is_snapshot) {
        invariant(txn->live_root_txn_list != nullptr);
        txn->live_root_txn_list->destroy();
        toku_free(txn->live_root_txn_list);
    }
    return;
}

static void toku_txn_manager_clone_state_for_gc_unlocked(
    TXN_MANAGER txn_manager,
    xid_omt_t* snapshot_xids,
    rx_omt_t* referenced_xids,
    xid_omt_t* live_root_txns
    )
{
    TXNID* snapshot_xids_array = NULL;
    XMALLOC_N(txn_manager->num_snapshots, snapshot_xids_array);
    TOKUTXN curr_txn = txn_manager->snapshot_head;
    uint32_t curr_index = 0;
    while (curr_txn != NULL) {
        snapshot_xids_array[curr_index] = curr_txn->snapshot_txnid64;
        curr_txn = curr_txn->snapshot_next;
        curr_index++;
    }
    snapshot_xids->create_steal_sorted_array(
        &snapshot_xids_array, 
        txn_manager->num_snapshots,
        txn_manager->num_snapshots
        );
    
    referenced_xids->clone(txn_manager->referenced_xids);
    setup_live_root_txn_list(&txn_manager->live_root_ids, live_root_txns);  
}

void toku_txn_manager_clone_state_for_gc(
    TXN_MANAGER txn_manager,
    xid_omt_t* snapshot_xids,
    rx_omt_t* referenced_xids,
    xid_omt_t* live_root_txns
    )
{
    txn_manager_lock(txn_manager);
    toku_txn_manager_clone_state_for_gc_unlocked(
        txn_manager, 
        snapshot_xids, 
        referenced_xids, 
        live_root_txns
        );
    txn_manager_unlock(txn_manager);
}

void txn_manager_state::init() {
    invariant(!initialized);
    invariant_notnull(txn_manager);
    toku_txn_manager_clone_state_for_gc(
        txn_manager,
        &snapshot_xids,
        &referenced_xids,
        &live_root_txns
        );
    initialized = true;
}

void toku_txn_manager_id2txn_unlocked(TXN_MANAGER txn_manager, TXNID_PAIR txnid, TOKUTXN *result) {
    TOKUTXN txn;
    int r = txn_manager->live_root_txns.find_zero<TXNID, find_by_xid>(txnid.parent_id64, &txn, nullptr);
    if (r==0) {
        assert(txn->txnid.parent_id64 == txnid.parent_id64);
        *result = txn;
    }
    else {
        assert(r==DB_NOTFOUND);
        // If there is no txn, then we treat it as the null txn.
        *result = NULL;
    }
}

int toku_txn_manager_get_root_txn_from_xid (TXN_MANAGER txn_manager, TOKU_XA_XID *xid, DB_TXN **txnp) {
    txn_manager_lock(txn_manager);
    int ret_val = 0;
    int num_live_txns = txn_manager->live_root_txns.size();
    for (int i = 0; i < num_live_txns; i++) {
        TOKUTXN txn;
        {
            int r = txn_manager->live_root_txns.fetch(i, &txn);
            assert_zero(r);
        }
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
    txn_manager_unlock(txn_manager);
    return ret_val;
}

uint32_t toku_txn_manager_num_live_root_txns(TXN_MANAGER txn_manager) {
    int ret_val = 0;
    txn_manager_lock(txn_manager);
    ret_val = txn_manager->live_root_txns.size();
    txn_manager_unlock(txn_manager);
    return ret_val;
}

static int txn_manager_iter(
    TXN_MANAGER txn_manager, 
    txn_mgr_iter_callback cb,
    void* extra,
    bool just_root_txns
    ) 
{
    int r = 0;
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    uint32_t size = txn_manager->live_root_txns.size();
    for (uint32_t i = 0; i < size; i++) {
        TOKUTXN curr_txn = NULL;
        r = txn_manager->live_root_txns.fetch(i, &curr_txn);
        assert_zero(r);
        if (just_root_txns) {
            r = cb(curr_txn, extra);
        }
        else {
            r = curr_txn->child_manager->iterate(cb, extra);
        }
        if (r) {
            break;
        }
    }
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
    return r;
}

int toku_txn_manager_iter_over_live_txns(
    TXN_MANAGER txn_manager, 
    txn_mgr_iter_callback cb,
    void* extra
    ) 
{
    return txn_manager_iter(
        txn_manager,
        cb,
        extra,
        false
        );
}

int toku_txn_manager_iter_over_live_root_txns(
    TXN_MANAGER txn_manager, 
    txn_mgr_iter_callback cb,
    void* extra
    )
{
    return txn_manager_iter(
        txn_manager,
        cb,
        extra,
        true
        );
}


//
// This function is called only via env_txn_xa_recover and env_txn_recover.
// See comments for those functions to understand assumptions that 
// can be made when calling this function. Namely, that the system is 
// quiescant, in that we are right after recovery and before user operations
// commence.
//
// Another key assumption made here is that only root transactions
// may be prepared and that child transactions cannot be prepared.
// This assumption is made by the fact that we iterate over the live root txns
// to find prepared transactions.
//
// I (Zardosht), don't think we take advantage of this fact, as we are holding
// the txn_manager_lock in this function, but in the future we might want
// to take these assumptions into account.
//
int toku_txn_manager_recover_root_txn (
    TXN_MANAGER txn_manager, 
    struct tokulogger_preplist preplist[/*count*/], 
    long count, 
    long *retp, /*out*/ 
    uint32_t flags
    )
{
    int ret_val = 0;
    txn_manager_lock(txn_manager);
    uint32_t num_txns_returned = 0;
    // scan through live root txns to find
    // prepared transactions and return them
    uint32_t size = txn_manager->live_root_txns.size();
    if (flags==DB_FIRST) {
        txn_manager->last_xid_seen_for_recover = TXNID_NONE;
    } 
    else if (flags!=DB_NEXT) { 
        ret_val = EINVAL;
        goto exit;
    }
    for (uint32_t i = 0; i < size; i++) {
        TOKUTXN curr_txn = NULL;
        txn_manager->live_root_txns.fetch(i, &curr_txn);
        // skip over TOKUTXNs whose txnid64 is too small, meaning
        // we have already processed them.
        if (curr_txn->txnid.parent_id64 <= txn_manager->last_xid_seen_for_recover) {
            continue;
        }
        if (curr_txn->state == TOKUTXN_PREPARING) {
            assert(curr_txn->container_db_txn);
            preplist[num_txns_returned].txn = curr_txn->container_db_txn;
            preplist[num_txns_returned].xid = curr_txn->xa_xid;
            txn_manager->last_xid_seen_for_recover = curr_txn->txnid.parent_id64;
            num_txns_returned++;
        }
        txn_manager->last_xid_seen_for_recover = curr_txn->txnid.parent_id64;
        // if we found the maximum number of prepared transactions we are
        // allowed to find, then break
        if (num_txns_returned >= count) {
            break;
        }
    }
    invariant(num_txns_returned <= count);
    *retp = num_txns_returned;
    ret_val = 0;
exit:
    txn_manager_unlock(txn_manager);
    return ret_val;
}

static void txn_manager_lock(TXN_MANAGER txn_manager) {
    toku_mutex_lock(&txn_manager->txn_manager_lock);
}

static void txn_manager_unlock(TXN_MANAGER txn_manager) {
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
}

void toku_txn_manager_suspend(TXN_MANAGER txn_manager) {
    txn_manager_lock(txn_manager);
}

void toku_txn_manager_resume(TXN_MANAGER txn_manager) {
    txn_manager_unlock(txn_manager);
}

void
toku_txn_manager_set_last_xid_from_logger(TXN_MANAGER txn_manager, TXNID last_xid) {
    invariant(txn_manager->last_xid == TXNID_NONE);
    txn_manager->last_xid = last_xid;
}

void
toku_txn_manager_set_last_xid_from_recovered_checkpoint(TXN_MANAGER txn_manager, TXNID last_xid) {
    txn_manager->last_xid = last_xid;
}

TXNID
toku_txn_manager_get_last_xid(TXN_MANAGER mgr) {
    txn_manager_lock(mgr);
    TXNID last_xid = mgr->last_xid;
    txn_manager_unlock(mgr);
    return last_xid;
}

bool 
toku_txn_manager_txns_exist(TXN_MANAGER mgr) {
    txn_manager_lock(mgr);
    bool retval = mgr->live_root_txns.size() > 0;
    txn_manager_unlock(mgr);
    return retval;
}


// Test-only function
void
toku_txn_manager_increase_last_xid(TXN_MANAGER mgr, uint64_t increment) {
    txn_manager_lock(mgr);
    mgr->last_xid += increment;
    txn_manager_unlock(mgr);
}

