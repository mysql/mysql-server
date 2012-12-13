/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef TOKU_LOCK_REQUEST_UNIT_TEST_H
#define TOKU_LOCK_REQUEST_UNIT_TEST_H

#include "test.h"
#include "locktree_unit_test.h"

#include "lock_request.h"

namespace toku {

class lock_request_unit_test {
public:
    // create and set the object's internals, destroy should not crash.
    void test_create_destroy(void);

    // make setting keys and getting them back works properly.
    // at a high level, we want to make sure keys are copied
    // when appropriate and plays nice with +/- infinity.
    void test_get_set_keys(void);

    // starting a lock request without immediate success should get
    // stored in the lock request set as pending.
    void test_start_pending(void);

    // make sure deadlocks are detected when a lock request starts
    void test_start_deadlock(void);

private:
    // releases a single range lock and retries all lock requests.
    // this is kind of like what the ydb layer does, except that
    // the ydb layer releases all of a txn's locks at once using
    // lt->release_locks(), not individually using lt->remove_overlapping_locks_for_txnid).
    void release_lock_and_retry_requests(locktree *lt,
            TXNID txnid, const DBT *left_key, const DBT * right_key) {
        locktree_unit_test::locktree_test_release_lock(lt, txnid, left_key, right_key);
        lock_request::retry_all_lock_requests(lt);
    }
};

}

#endif
