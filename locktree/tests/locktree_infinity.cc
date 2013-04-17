/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "locktree_unit_test.h"

namespace toku {

// test that ranges with infinite endpoints work
void locktree_unit_test::test_infinity(void) {
    locktree::manager mgr;
    mgr.create(nullptr, nullptr, nullptr, nullptr);
    DESCRIPTOR desc = nullptr;
    DICTIONARY_ID dict_id = { 1 };
    locktree *lt = mgr.get_lt(dict_id, desc, compare_dbts, nullptr);

    int r;
    TXNID txnid_a = 1001;
    TXNID txnid_b = 2001;
    const DBT *zero = get_dbt(0);
    const DBT *one = get_dbt(1);
    const DBT *two = get_dbt(2);
    const DBT *five = get_dbt(5);
    const DBT min_int = min_dbt();
    const DBT max_int = max_dbt();

    // txn A will lock -inf, 5.
    r = lt->acquire_write_lock(txnid_a, toku_dbt_negative_infinity(), five, nullptr);
    invariant(r == 0);
    // txn B will fail to get any lock <= 5, even min_int
    r = lt->acquire_write_lock(txnid_b, five, five, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, zero, one, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &min_int, &min_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, toku_dbt_negative_infinity(), &min_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);

    lt->remove_overlapping_locks_for_txnid(txnid_a, toku_dbt_negative_infinity(), five);

    // txn A will lock 1, +inf
    r = lt->acquire_write_lock(txnid_a, one, toku_dbt_positive_infinity(), nullptr);
    invariant(r == 0);
    // txn B will fail to get any lock >= 1, even max_int
    r = lt->acquire_write_lock(txnid_b, one, one, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, two, five, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &max_int, &max_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &max_int, toku_dbt_positive_infinity(), nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);

    lt->remove_overlapping_locks_for_txnid(txnid_a, toku_dbt_negative_infinity(), five);

    // txn A will lock -inf, +inf
    r = lt->acquire_write_lock(txnid_a, toku_dbt_negative_infinity(), toku_dbt_positive_infinity(), nullptr);
    invariant(r == 0);
    // txn B will fail to get any lock
    r = lt->acquire_write_lock(txnid_b, zero, one, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, two, five, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &min_int, &min_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &min_int, &max_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &max_int, &max_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, toku_dbt_negative_infinity(), toku_dbt_negative_infinity(), nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, toku_dbt_negative_infinity(), toku_dbt_positive_infinity(), nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, toku_dbt_positive_infinity(), toku_dbt_positive_infinity(), nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);

    lt->remove_overlapping_locks_for_txnid(txnid_a, toku_dbt_negative_infinity(), toku_dbt_positive_infinity());

    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::locktree_unit_test test;
    test.test_infinity();
    return 0;
}
