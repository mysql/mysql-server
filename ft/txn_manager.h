/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKUTXN_MANAGER_H
#define TOKUTXN_MANAGER_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

struct txn_manager;

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
    OMT* snapshot_xids,
    OMT* referenced_xids,
    OMT* live_root_txns
    );

void toku_txn_manager_id2txn_unlocked(TXN_MANAGER txn_manager, TXNID txnid, TOKUTXN *result);
void toku_txn_manager_id2txn (TXN_MANAGER txn_manager, TXNID txnid, TOKUTXN *result);

int toku_txn_manager_get_txn_from_xid (TXN_MANAGER txn_manager, TOKU_XA_XID *xid, DB_TXN **txnp);

u_int32_t toku_txn_manager_num_live_txns(TXN_MANAGER txn_manager);

int toku_txn_manager_iter_over_live_txns(
    TXN_MANAGER txn_manager,
    int (*f)(OMTVALUE, u_int32_t, void*),
    void* v
    );

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

TXNID toku_get_youngest_live_list_txnid_for(TXNID xc, OMT snapshot_txnids, OMT referenced_xids);
#if defined(__cplusplus) || defined(__cilkplusplus)
}
#endif

#endif //TOKUTXN_H
