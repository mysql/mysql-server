/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKU_TXN_CHILD_MANAGER_H
#define TOKU_TXN_CHILD_MANAGER_H

#ident "$Id: rollback.h 49033 2012-10-17 18:48:30Z zardosht $"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "txn_manager.h"

class txn_child_manager {
public:
    void init (TOKUTXN root);
    void destroy();
    void start_child_txn_for_recovery(TOKUTXN child, TOKUTXN parent, TXNID_PAIR txnid);
    void start_child_txn(TOKUTXN child, TOKUTXN parent);
    void finish_child_txn(TOKUTXN child);
    void suspend();
    void resume();
    void find_tokutxn_by_xid_unlocked(TXNID_PAIR xid, TOKUTXN* result);
    int iterate(txn_mgr_iter_callback cb, void* extra);

private:
    TXNID m_last_xid;
    TOKUTXN m_root;
    toku_mutex_t m_mutex;

friend class txn_child_manager_unit_test;
};


ENSURE_POD(txn_child_manager);

#endif // TOKU_TXN_CHILD_MANAGER_H
