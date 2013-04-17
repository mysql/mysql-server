/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "locktree_unit_test.h"

#include <toku_time.h>

__attribute__((__unused__))
static long current_time_usec(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_usec + t.tv_sec * 1000000;
}

namespace toku {

// test simple, non-overlapping read locks and then write locks
void locktree_unit_test::test_simple_lock(void) {
    locktree::manager mgr;
    mgr.create(nullptr, nullptr);
    DESCRIPTOR desc = nullptr;
    DICTIONARY_ID dict_id = { 1 };
    locktree *lt = mgr.get_lt(dict_id, desc, compare_dbts, nullptr);

    int r;
    TXNID txnid_a = 1001;
    TXNID txnid_b = 2001;
    TXNID txnid_c = 3001;
    TXNID txnid_d = 4001;
    const DBT *one = get_dbt(1);
    const DBT *two = get_dbt(2);
    const DBT *three = get_dbt(3);
    const DBT *four = get_dbt(4);

    for (int test_run = 0; test_run < 2; test_run++) {
        // test_run == 0 means test with read lock
        // test_run == 1 means test with write lock
#define ACQUIRE_LOCK(txn, left, right, conflicts) \
        test_run == 0 ? lt->acquire_read_lock(txn, left, right, conflicts) \
                      : lt->acquire_write_lock(txn, left, right, conflicts)

        // four txns, four points
        r = ACQUIRE_LOCK(txnid_a, one, one, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 1);
        r = ACQUIRE_LOCK(txnid_b, two, two, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 2);
        r = ACQUIRE_LOCK(txnid_c, three, three, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 3);
        r = ACQUIRE_LOCK(txnid_d, four, four, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 4);
        lt->remove_overlapping_locks_for_txnid(txnid_a, one, one);
        invariant(num_row_locks(lt) == 3);
        lt->remove_overlapping_locks_for_txnid(txnid_b, two, two);
        invariant(num_row_locks(lt) == 2);
        lt->remove_overlapping_locks_for_txnid(txnid_c, three, three);
        invariant(num_row_locks(lt) == 1);
        lt->remove_overlapping_locks_for_txnid(txnid_d, four, four);
        invariant(num_row_locks(lt) == 0);

        // two txns, two ranges
        r = ACQUIRE_LOCK(txnid_c, one, two, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 1);
        r = ACQUIRE_LOCK(txnid_b, three, four, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 2);
        lt->remove_overlapping_locks_for_txnid(txnid_c, one, two);
        invariant(num_row_locks(lt) == 1);
        lt->remove_overlapping_locks_for_txnid(txnid_b, three, four);
        invariant(num_row_locks(lt) == 0);

        // one txn, one range, one point
        r = ACQUIRE_LOCK(txnid_a, two, three, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 1);
        r = ACQUIRE_LOCK(txnid_a, four, four, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 2);
        lt->remove_overlapping_locks_for_txnid(txnid_a, two, three);
        invariant(num_row_locks(lt) == 1);
        lt->remove_overlapping_locks_for_txnid(txnid_a, four, four);
        invariant(num_row_locks(lt) == 0);

        // two txns, one range, one point
        r = ACQUIRE_LOCK(txnid_c, three, four, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 1);
        r = ACQUIRE_LOCK(txnid_d, one, one, nullptr);
        invariant(r == 0);
        invariant(num_row_locks(lt) == 2);
        lt->remove_overlapping_locks_for_txnid(txnid_c, three, four);
        invariant(num_row_locks(lt) == 1);
        lt->remove_overlapping_locks_for_txnid(txnid_d, one, one);
        invariant(num_row_locks(lt) == 0);

#undef ACQUIRE_LOCK
    }

    const int64_t num_locks = 10000;

    int64_t *keys = (int64_t *) toku_malloc(num_locks * sizeof(int64_t));
    for (int64_t i = 0; i < num_locks; i++) {
        keys[i] = i; 
    }
    for (int64_t i = 0; i < num_locks; i++) {
        int64_t k = rand() % num_locks; 
        int64_t tmp = keys[k];
        keys[k] = keys[i];
        keys[i] = tmp;
    }


    r = mgr.set_max_lock_memory((num_locks + 1) * 500);
    invariant_zero(r);

    DBT k;
    k.ulen = 0;
    k.size = sizeof(keys[0]);
    k.flags = DB_DBT_USERMEM;

    for (int64_t i = 0; i < num_locks; i++) {
        k.data = (void *) &keys[i];
        r = lt->acquire_read_lock(txnid_a, &k, &k, nullptr);
        invariant(r == 0);
    }

    for (int64_t i = 0; i < num_locks; i++) {
        k.data = (void *) &keys[i];
        lt->remove_overlapping_locks_for_txnid(txnid_a, &k, &k);
    }

    toku_free(keys);

    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::locktree_unit_test test;
    test.test_simple_lock();
    return 0;
}
