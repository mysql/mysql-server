/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKUTXN_MANAGER_H
#define TOKUTXN_MANAGER_H

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <util/omt.h>
#include "fttypes.h"
#include <portability/toku_pthread.h>
#include <util/omt.h>

struct referenced_xid_tuple {
    TXNID begin_id;
    TXNID end_id;
    uint32_t references;
};

typedef toku::omt<TOKUTXN> txn_omt_t;
typedef toku::omt<TXNID> xid_omt_t;
typedef toku::omt<struct referenced_xid_tuple, struct referenced_xid_tuple *> rx_omt_t;

struct txn_manager {
    toku_mutex_t txn_manager_lock;  // a lock protecting this object
    txn_omt_t live_root_txns; // a sorted tree.
    xid_omt_t live_root_ids;    //contains TXNID x | x is snapshot txn
    TOKUTXN snapshot_head;
    TOKUTXN snapshot_tail;
    uint32_t num_snapshots;
    // Contains 3-tuples: (TXNID begin_id, TXNID end_id, uint64_t num_live_list_references)
    //                    for committed root transaction ids that are still referenced by a live list.
    rx_omt_t referenced_xids;

    TXNID last_xid;
    TXNID last_xid_seen_for_recover;
};


void toku_txn_manager_init(TXN_MANAGER* txn_manager);
void toku_txn_manager_destroy(TXN_MANAGER txn_manager);

TXNID toku_txn_manager_get_oldest_living_xid(TXN_MANAGER txn_manager);

void toku_txn_manager_handle_snapshot_create_for_child_txn(
    TOKUTXN txn,
    TXN_MANAGER txn_manager,
    TXN_SNAPSHOT_TYPE snapshot_type
    );
void toku_txn_manager_handle_snapshot_destroy_for_child_txn(
    TOKUTXN txn,
    TXN_MANAGER txn_manager,
    TXN_SNAPSHOT_TYPE snapshot_type
    );


// Assign a txnid. Log the txn begin in the recovery log. Initialize the txn live lists.
void toku_txn_manager_start_txn(
    TOKUTXN txn,
    TXN_MANAGER txn_manager,
    TXN_SNAPSHOT_TYPE snapshot_type,
    bool read_only
    );

void toku_txn_manager_start_txn_for_recovery(
    TOKUTXN txn,
    TXN_MANAGER txn_manager,
    TXNID xid
    );

void toku_txn_manager_finish_txn(TXN_MANAGER txn_manager, TOKUTXN txn);

void toku_txn_manager_clone_state_for_gc(
    TXN_MANAGER txn_manager,
    xid_omt_t* snapshot_xids,
    rx_omt_t* referenced_xids,
    xid_omt_t* live_root_txns
    );

void toku_txn_manager_id2txn_unlocked(TXN_MANAGER txn_manager, TXNID_PAIR txnid, TOKUTXN *result);

// Returns a root txn associated with xid. The system as a whole
// assumes that only root txns get prepared, adn therefore only
// root txns will have XIDs associated with them.
int toku_txn_manager_get_root_txn_from_xid (TXN_MANAGER txn_manager, TOKU_XA_XID *xid, DB_TXN **txnp);

uint32_t toku_txn_manager_num_live_root_txns(TXN_MANAGER txn_manager);

typedef int (*txn_mgr_iter_callback)(TOKUTXN txn, void* extra);

int toku_txn_manager_iter_over_live_txns(
    TXN_MANAGER txn_manager, 
    txn_mgr_iter_callback cb,
    void* extra
    );

int toku_txn_manager_iter_over_live_root_txns(
    TXN_MANAGER txn_manager, 
    txn_mgr_iter_callback cb,
    void* extra
    );

int toku_txn_manager_recover_root_txn(
    TXN_MANAGER txn_manager,
    struct tokulogger_preplist preplist[/*count*/],
    long count,
    long *retp, /*out*/
    uint32_t flags
    );

void toku_txn_manager_suspend(TXN_MANAGER txn_manager);
void toku_txn_manager_resume(TXN_MANAGER txn_manager);

void toku_txn_manager_set_last_xid_from_logger(TXN_MANAGER txn_manager, TXNID last_xid);
void toku_txn_manager_set_last_xid_from_recovered_checkpoint(TXN_MANAGER txn_manager, TXNID last_xid);
TXNID toku_txn_manager_get_last_xid(TXN_MANAGER mgr);

bool toku_txn_manager_txns_exist(TXN_MANAGER mgr);

// Test-only function
void toku_txn_manager_increase_last_xid(TXN_MANAGER mgr, uint64_t increment);

TXNID toku_get_youngest_live_list_txnid_for(TXNID xc, const xid_omt_t &snapshot_txnids, const rx_omt_t &referenced_xids);

#endif // TOKUTXN_MANAGER_H
