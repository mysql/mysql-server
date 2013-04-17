/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "lock_request_unit_test.h"

namespace toku {

// make setting keys and getting them back works properly.
// at a high level, we want to make sure keys are copied
// when appropriate and plays nice with +/- infinity.
void lock_request_unit_test::test_get_set_keys(void) {
    lock_request request;
    const uint64_t lock_wait_time = 10;
    request.create(lock_wait_time);

    locktree *const null_lt = nullptr;

    TXNID txnid_a = 1001;

    const DBT *one = get_dbt(1);
    const DBT *two = get_dbt(2);
    const DBT *neg_inf = toku_dbt_negative_infinity();
    const DBT *pos_inf = toku_dbt_negative_infinity();

    // request should not copy dbts for neg/pos inf, so get_left
    // and get_right should return the same pointer given
    request.set(null_lt, txnid_a, neg_inf, pos_inf, lock_request::type::WRITE);
    invariant(request.get_left_key() == neg_inf);
    invariant(request.get_right_key() == pos_inf);

    // request should make copies of non-infinity-valued keys.
    request.set(null_lt, txnid_a, neg_inf, one, lock_request::type::WRITE);
    invariant(request.get_left_key() == neg_inf);
    invariant(request.get_right_key() == one);

    request.set(null_lt, txnid_a, two, pos_inf, lock_request::type::WRITE);
    invariant(request.get_left_key() == two);
    invariant(request.get_right_key() == pos_inf);

    request.set(null_lt, txnid_a, one, two, lock_request::type::WRITE);
    invariant(request.get_left_key() == one);
    invariant(request.get_right_key() == two);

    request.destroy();
}

}

int main(void) {
    toku::lock_request_unit_test test;
    test.test_get_set_keys();
    return 0;
}

