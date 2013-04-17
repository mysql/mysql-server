/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "locktree_unit_test.h"

namespace toku {

// test that the same txn can relock ranges it already owns
// ensure that existing read locks can be upgrading to
// write locks if overlapping and ensure that existing read
// or write locks are consolidated by overlapping relocks.
void locktree_unit_test::test_overlapping_relock(void) {
    locktree::manager mgr;
    mgr.create(nullptr, nullptr);
    DESCRIPTOR desc = nullptr;
    DICTIONARY_ID dict_id = { 1 };
    locktree *lt = mgr.get_lt(dict_id, desc, compare_dbts, nullptr);

    const DBT *zero = get_dbt(0);
    const DBT *one = get_dbt(1);
    const DBT *two = get_dbt(2);
    const DBT *three = get_dbt(3);
    const DBT *four = get_dbt(4);
    const DBT *five = get_dbt(5);

    int r;
    TXNID txnid_a = 1001;

    for (int test_run = 0; test_run < 2; test_run++) {
        // test_run == 0 means test with read lock
        // test_run == 1 means test with write lock
#define ACQUIRE_LOCK(txn, left, right, conflicts) \
        test_run == 0 ? lt->acquire_read_lock(txn, left, right, conflicts) \
                      : lt->acquire_write_lock(txn, left, right, conflicts)

        // lock [1,1] and [2,2]. then lock [1,2].
        // ensure only [1,2] exists in the tree
        r = ACQUIRE_LOCK(txnid_a, one, one, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 1);
        r = ACQUIRE_LOCK(txnid_a, two, two, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 2);
        r = ACQUIRE_LOCK(txnid_a, one, two, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 1);

        struct verify_fn_obj {
            TXNID expected_txnid;
            keyrange *expected_range;
            comparator *cmp;
            bool fn(const keyrange &range, TXNID txnid) {
                invariant(txnid == expected_txnid);
                keyrange::comparison c = range.compare(cmp, *expected_range);
                invariant(c == keyrange::comparison::EQUALS);
                return true;
            }
        } verify_fn;
        verify_fn.cmp = lt->m_cmp;

        keyrange range;
        range.create(one, two);
        verify_fn.expected_txnid = txnid_a;
        verify_fn.expected_range = &range;
        locktree_iterate<verify_fn_obj>(lt, &verify_fn);

        // unlocking [1,1] should remove the only range,
        // the other unlocks shoudl do nothing.
        invariant(num_row_locks(lt) == 1);
        lt->remove_overlapping_locks_for_txnid(txnid_a, one, one);
        invariant(num_row_locks(lt) == 0);
        lt->remove_overlapping_locks_for_txnid(txnid_a, two, two);
        invariant(num_row_locks(lt) == 0);
        lt->remove_overlapping_locks_for_txnid(txnid_a, one, two);
        invariant(num_row_locks(lt) == 0);

        // try overlapping from the right
        r = ACQUIRE_LOCK(txnid_a, one, three, nullptr);
        invariant(num_row_locks(lt) == 1);
        r = ACQUIRE_LOCK(txnid_a, two, five, nullptr);
        invariant(num_row_locks(lt) == 1);
        verify_fn.expected_txnid = txnid_a;
        range.create(one, five);
        verify_fn.expected_range = &range;
        locktree_iterate<verify_fn_obj>(lt, &verify_fn);

        // now overlap from the left
        r = ACQUIRE_LOCK(txnid_a, zero, four, nullptr);
        invariant(num_row_locks(lt) == 1);
        verify_fn.expected_txnid = txnid_a;
        range.create(zero, five);
        verify_fn.expected_range = &range;
        locktree_iterate<verify_fn_obj>(lt, &verify_fn);

        // now relock in a range that is already dominated
        r = ACQUIRE_LOCK(txnid_a, five, five, nullptr);
        invariant(num_row_locks(lt) == 1);
        verify_fn.expected_txnid = txnid_a;
        range.create(zero, five);
        verify_fn.expected_range = &range;
        locktree_iterate<verify_fn_obj>(lt, &verify_fn);

        // release one of the locks we acquired. this should clean up the whole range.
        lt->remove_overlapping_locks_for_txnid(txnid_a, zero, four);
        invariant(num_row_locks(lt) == 0);

#undef ACQUIRE_LOCK
    }

    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::locktree_unit_test test;
    test.test_overlapping_relock();
    return 0;
}
