/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "lock_request_unit_test.h"

namespace toku {

// make sure deadlocks are detected when a lock request starts
void lock_request_unit_test::test_start_deadlock(void) {
    int r;
    locktree::manager mgr;
    locktree *lt;
    // something short
    const uint64_t lock_wait_time = 10;

    mgr.create(nullptr, nullptr);
    DICTIONARY_ID dict_id = { 1 };
    lt = mgr.get_lt(dict_id, nullptr, compare_dbts, nullptr);

    TXNID txnid_a = 1001;
    TXNID txnid_b = 2001;
    TXNID txnid_c = 3001;
    lock_request request_a;
    lock_request request_b;
    lock_request request_c;
    request_a.create(lock_wait_time);
    request_b.create(lock_wait_time);
    request_c.create(lock_wait_time);

    const DBT *one = get_dbt(1);
    const DBT *two = get_dbt(2);

    // start and succeed 1,1 for A and 2,2 for B.
    request_a.set(lt, txnid_a, one, one, lock_request::type::WRITE);
    r = request_a.start();
    invariant_zero(r);
    request_b.set(lt, txnid_b, two, two, lock_request::type::WRITE);
    r = request_b.start();
    invariant_zero(r);

    // txnid A should not be granted a lock on 2,2, so it goes pending.
    request_a.set(lt, txnid_a, two, two, lock_request::type::WRITE);
    r = request_a.start();
    invariant(r == DB_LOCK_NOTGRANTED);

    // if txnid B wants a lock on 1,1 it should deadlock with A
    request_b.set(lt, txnid_b, one, one, lock_request::type::WRITE);
    r = request_b.start();
    invariant(r == DB_LOCK_DEADLOCK);

    // txnid C should not deadlock on either of these - it should just time out.
    request_c.set(lt, txnid_c, one, one, lock_request::type::WRITE);
    r = request_c.start();
    invariant(r == DB_LOCK_NOTGRANTED);
    r = request_c.wait();
    invariant(r == DB_LOCK_NOTGRANTED);
    request_c.set(lt, txnid_c, two, two, lock_request::type::WRITE);
    r = request_c.start();
    invariant(r == DB_LOCK_NOTGRANTED);
    r = request_c.wait();
    invariant(r == DB_LOCK_NOTGRANTED);

    // release locks for A and B, then wait on A's request which should succeed
    // since B just unlocked and should have completed A's pending request.
    release_lock_and_retry_requests(lt, txnid_a, one, one);
    release_lock_and_retry_requests(lt, txnid_b, two, two);
    r = request_a.wait();
    invariant_zero(r);
    release_lock_and_retry_requests(lt, txnid_a, two, two);

    request_a.destroy();
    request_b.destroy();
    request_c.destroy();
    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::lock_request_unit_test test;
    test.test_start_deadlock();
    return 0;
}

