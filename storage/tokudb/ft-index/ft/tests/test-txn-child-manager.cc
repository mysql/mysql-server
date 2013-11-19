/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: xid_lsn_independent.cc 49853 2012-11-12 04:26:30Z zardosht $"
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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
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
        TXN_SNAPSHOT_CHILD,
        false
        );
    CKERR(r);
    // test starting a child txn
    TOKUTXN child_txn = NULL;
    r = toku_txn_begin_txn(
        NULL,
        root_txn,
        &child_txn,
        logger,
        TXN_SNAPSHOT_CHILD,
        false
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
        TXN_SNAPSHOT_ROOT,
        false
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
        TXN_SNAPSHOT_ROOT,
        false
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
        TXN_SNAPSHOT_ROOT,
        false
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
        TXN_SNAPSHOT_ROOT,
        false
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
        true, // for recovery
        false // read_only
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
