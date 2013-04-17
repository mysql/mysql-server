/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: xid_lsn_independent.cc 49853 2012-11-12 04:26:30Z zardosht $"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

#include "toku_os.h"
#include "checkpoint.h"

#include "test-ft-txns.h"

static int txn_child_manager_test_cb(TOKUTXN txn, void* extra) {
    TOKUTXN* ptxn = (TOKUTXN *)extra;
    assert(txn == *ptxn);
    *ptxn = txn->child;
    return 0;
}

static int txn_child_manager_test_cb2(TOKUTXN txn, void* extra) {
    TOKUTXN extra_txn = (TOKUTXN)extra;
    if (txn == extra_txn) {
        return -1;
    }
    return 0;
}


class txn_child_manager_unit_test {
public:
    void run_test();
    void run_child_txn_test();
};

// simple test that verifies that creating a TXN_CHILD_SNAPSHOT tokutxn
// creates its own snapshot
void txn_child_manager_unit_test::run_child_txn_test() {
    TOKULOGGER logger;
    CACHETABLE ct;
    int r = 0;
    test_setup(TOKU_TEST_FILENAME, &logger, &ct);
    // create the root transaction
    TOKUTXN root_txn = NULL;
    r = toku_txn_begin_txn(
        (DB_TXN *)NULL,
        NULL,
        &root_txn,
        logger,
        TXN_SNAPSHOT_CHILD
        );
    CKERR(r);
    // test starting a child txn
    TOKUTXN child_txn = NULL;
    r = toku_txn_begin_txn(
        NULL,
        root_txn,
        &child_txn,
        logger,
        TXN_SNAPSHOT_CHILD
        );
    CKERR(r);

    // assert that the child has a later snapshot
    assert(child_txn->snapshot_txnid64 > root_txn->snapshot_txnid64);

    r = toku_txn_commit_txn(child_txn, true, NULL, NULL);
    CKERR(r);
    toku_txn_close_txn(child_txn);
    assert(root_txn->child == NULL);

    r = toku_txn_commit_txn(root_txn, true, NULL, NULL);
    CKERR(r);
    toku_txn_close_txn(root_txn);

    
    clean_shutdown(&logger, &ct);
}

void txn_child_manager_unit_test::run_test() {
    TOKULOGGER logger;
    CACHETABLE ct;
    int r = 0;
    test_setup(TOKU_TEST_FILENAME, &logger, &ct);
    // create the root transaction
    TOKUTXN root_txn = NULL;
    r = toku_txn_begin_txn(
        (DB_TXN *)NULL,
        NULL,
        &root_txn,
        logger,
        TXN_SNAPSHOT_ROOT
        );
    CKERR(r);
    txn_child_manager* cm = root_txn->child_manager;
    assert(cm == &root_txn->child_manager_s);
    assert(cm->m_root == root_txn);
    assert(cm->m_last_xid == TXNID_NONE);
    assert(root_txn->child == NULL);
    // this assumption implies our assumptions of child_id values below,
    // because the parent id cannot be the child id
    assert(root_txn->txnid.parent_id64 == 1);

    // test starting a child txn
    TOKUTXN child_txn = NULL;
    r = toku_txn_begin_txn(
        NULL,
        root_txn,
        &child_txn,
        logger,
        TXN_SNAPSHOT_ROOT
        );
    CKERR(r);
    assert(child_txn->child_manager == cm);
    assert(child_txn->parent == root_txn);
    assert(root_txn->child == child_txn);
    assert(child_txn->txnid.parent_id64 == root_txn->txnid.parent_id64);
    assert(child_txn->txnid.child_id64 == 2);
    assert(child_txn->live_root_txn_list == root_txn->live_root_txn_list);
    assert(child_txn->snapshot_txnid64 == root_txn->snapshot_txnid64);

    assert(cm->m_root == root_txn);
    assert(cm->m_last_xid == child_txn->txnid.child_id64);

    TOKUTXN grandchild_txn = NULL;
    r = toku_txn_begin_txn(
        NULL,
        child_txn,
        &grandchild_txn,
        logger,
        TXN_SNAPSHOT_ROOT
        );
    CKERR(r);
    assert(grandchild_txn->child_manager == cm);
    assert(grandchild_txn->parent == child_txn);
    assert(child_txn->child == grandchild_txn);
    assert(grandchild_txn->txnid.parent_id64 == root_txn->txnid.parent_id64);
    assert(grandchild_txn->txnid.child_id64 == 3);
    assert(grandchild_txn->live_root_txn_list == root_txn->live_root_txn_list);
    assert(grandchild_txn->snapshot_txnid64 == root_txn->snapshot_txnid64);

    assert(cm->m_root == root_txn);
    assert(cm->m_last_xid == grandchild_txn->txnid.child_id64);

    r = toku_txn_commit_txn(grandchild_txn, true, NULL, NULL);
    CKERR(r);
    toku_txn_close_txn(grandchild_txn);


    // now after closing one grandchild txn, open another one
    r = toku_txn_begin_txn(
        NULL,
        child_txn,
        &grandchild_txn,
        logger,
        TXN_SNAPSHOT_ROOT
        );
    CKERR(r);
    assert(grandchild_txn->child_manager == cm);
    assert(grandchild_txn->parent == child_txn);
    assert(child_txn->child == grandchild_txn);
    assert(grandchild_txn->txnid.parent_id64 == root_txn->txnid.parent_id64);
    assert(grandchild_txn->txnid.child_id64 == 4);
    assert(grandchild_txn->live_root_txn_list == root_txn->live_root_txn_list);
    assert(grandchild_txn->snapshot_txnid64 == root_txn->snapshot_txnid64);

    assert(cm->m_root == root_txn);
    assert(cm->m_last_xid == grandchild_txn->txnid.child_id64);


    TXNID_PAIR xid = {.parent_id64 = root_txn->txnid.parent_id64, .child_id64 = 100};
    TOKUTXN recovery_txn = NULL;
    r = toku_txn_begin_with_xid(
        grandchild_txn,
        &recovery_txn,
        logger,
        xid,
        TXN_SNAPSHOT_NONE,
        NULL,
        true // for recovery
        );

    assert(recovery_txn->child_manager == cm);
    assert(recovery_txn->parent == grandchild_txn);
    assert(grandchild_txn->child == recovery_txn);
    assert(recovery_txn->txnid.parent_id64 == root_txn->txnid.parent_id64);
    assert(recovery_txn->txnid.child_id64 == 100);
    // ensure that no snapshot is made
    assert(recovery_txn->live_root_txn_list == NULL);
    assert(recovery_txn->snapshot_txnid64 == TXNID_NONE);

    assert(cm->m_root == root_txn);
    assert(cm->m_last_xid == recovery_txn->txnid.child_id64);


    // now ensure that txn_child_manager::find_tokutxn_by_xid_unlocked works
    TOKUTXN found_txn = NULL;
    // first ensure that a dummy TXNID_PAIR cannot be found
    TXNID_PAIR dummy_pair = { .parent_id64 = root_txn->txnid.parent_id64, .child_id64 = 1000};
    cm->find_tokutxn_by_xid_unlocked(dummy_pair, &found_txn);
    assert(found_txn == NULL);
    cm->find_tokutxn_by_xid_unlocked(root_txn->txnid, &found_txn);
    assert(found_txn == root_txn);
    cm->find_tokutxn_by_xid_unlocked(child_txn->txnid, &found_txn);
    assert(found_txn == child_txn);
    cm->find_tokutxn_by_xid_unlocked(grandchild_txn->txnid, &found_txn);
    assert(found_txn == grandchild_txn);
    cm->find_tokutxn_by_xid_unlocked(recovery_txn->txnid, &found_txn);
    assert(found_txn == recovery_txn);


    // now ensure that the iterator works
    found_txn = root_txn;
    r = cm->iterate(txn_child_manager_test_cb, &found_txn);
    CKERR(r);
    assert(found_txn == NULL);

    // now test that iterator properly stops
    found_txn = child_txn;
    r = cm->iterate(txn_child_manager_test_cb2, found_txn);
    assert(r == -1);

    r = toku_txn_commit_txn(recovery_txn, true, NULL, NULL);
    CKERR(r);
    toku_txn_close_txn(recovery_txn);
    assert(grandchild_txn->child == NULL);

    r = toku_txn_commit_txn(grandchild_txn, true, NULL, NULL);
    CKERR(r);
    toku_txn_close_txn(grandchild_txn);    
    assert(child_txn->child == NULL);

    r = toku_txn_commit_txn(child_txn, true, NULL, NULL);
    CKERR(r);
    toku_txn_close_txn(child_txn);
    assert(root_txn->child == NULL);

    r = toku_txn_commit_txn(root_txn, true, NULL, NULL);
    CKERR(r);
    toku_txn_close_txn(root_txn);

    
    clean_shutdown(&logger, &ct);
}

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    txn_child_manager_unit_test foo;
    foo.run_test();
    return 0;
}
