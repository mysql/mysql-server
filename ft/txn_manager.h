/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKUTXN_MANAGER_H
#define TOKUTXN_MANAGER_H

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <toku_pthread.h>
#include "omt.h"
#include "omt-tmpl.h"
#include "fttypes.h"

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
    txn_omt_t live_txns; // a sorted tree.  Old comment said should be a hashtable.  Do we still want that?
    xid_omt_t live_root_txns; // a sorted tree.
    xid_omt_t snapshot_txnids;    //contains TXNID x | x is snapshot txn
    // Contains 3-tuples: (TXNID begin_id, TXNID end_id, uint64_t num_live_list_references)
    //                    for committed root transaction ids that are still referenced by a live list.
    rx_omt_t referenced_xids;
    struct toku_list prepared_txns; // transactions that have been prepared and are unresolved, but have not been returned through txn_recover.
    struct toku_list prepared_and_returned_txns; // transactions that have been prepared and unresolved, and have been returned through txn_recover.  We need this list so that we can restart the recovery.

    toku_cond_t wait_for_unpin_of_txn;
    TXNID last_xid;
};


void toku_txn_manager_init(TXN_MANAGER* txn_manager);
void toku_txn_manager_destroy(TXN_MANAGER txn_manager);

TXNID toku_txn_manager_get_oldest_living_xid(TXN_MANAGER txn_manager);

// Assign a txnid. Log the txn begin in the recovery log. Initialize the txn live lists.
// Also, create the txn.
int toku_txn_manager_start_txn(
    TOKUTXN *txnp,
    TXN_MANAGER txn_manager,
    TOKUTXN parent,
    TOKULOGGER logger,
    TXNID xid,
    TXN_SNAPSHOT_TYPE snapshot_type,
    DB_TXN *container_db_txn,
    bool for_recovery);
void toku_txn_manager_finish_txn(TXN_MANAGER txn_manager, TOKUTXN txn);

void toku_txn_manager_clone_state_for_gc(
    TXN_MANAGER txn_manager,
    xid_omt_t &snapshot_xids,
    rx_omt_t &referenced_xids,
    xid_omt_t &live_root_txns
    );

void toku_txn_manager_id2txn_unlocked(TXN_MANAGER txn_manager, TXNID txnid, TOKUTXN *result);
void toku_txn_manager_id2txn (TXN_MANAGER txn_manager, TXNID txnid, TOKUTXN *result);

int toku_txn_manager_get_txn_from_xid (TXN_MANAGER txn_manager, TOKU_XA_XID *xid, DB_TXN **txnp);

uint32_t toku_txn_manager_num_live_txns(TXN_MANAGER txn_manager);


template<typename iterate_extra_t,
         int (*f)(const TOKUTXN &, const uint32_t, iterate_extra_t *const)>
int toku_txn_manager_iter_over_live_txns(
    TXN_MANAGER txn_manager, 
    iterate_extra_t *const v
    ) 
{
    int r = 0;
    toku_mutex_lock(&txn_manager->txn_manager_lock);
    r = txn_manager->live_txns.iterate<iterate_extra_t, f>(v);
    toku_mutex_unlock(&txn_manager->txn_manager_lock);
    return r;
}

void toku_txn_manager_add_prepared_txn(TXN_MANAGER txn_manager, TOKUTXN txn);
void toku_txn_manager_note_abort_txn(TXN_MANAGER txn_manager, TOKUTXN txn);
void toku_txn_manager_note_commit_txn(TXN_MANAGER txn_manager, TOKUTXN txn);

int toku_txn_manager_recover_txn(
    TXN_MANAGER txn_manager,
    struct tokulogger_preplist preplist[/*count*/],
    long count,
    long *retp, /*out*/
    u_int32_t flags
    );

void toku_txn_manager_pin_live_txn_unlocked(TXN_MANAGER txn_manager, TOKUTXN txn);
void toku_txn_manager_unpin_live_txn_unlocked(TXN_MANAGER txn_manager, TOKUTXN txn);

void toku_txn_manager_suspend(TXN_MANAGER txn_manager);
void toku_txn_manager_resume(TXN_MANAGER txn_manager);

void toku_txn_manager_set_last_xid_from_logger(TXN_MANAGER txn_manager, TXNID last_xid);
void toku_txn_manager_set_last_xid_from_recovered_checkpoint(TXN_MANAGER txn_manager, TXNID last_xid);
TXNID toku_txn_manager_get_last_xid(TXN_MANAGER mgr);

// Test-only function
void toku_txn_manager_increase_last_xid(TXN_MANAGER mgr, uint64_t increment);

TXNID toku_get_youngest_live_list_txnid_for(TXNID xc, const xid_omt_t &snapshot_txnids, const rx_omt_t &referenced_xids);

#endif //TOKUTXN_H
