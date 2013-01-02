/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <portability/toku_pthread.h>

#include "concurrent_tree_unit_test.h"

namespace toku {

// This is intended to be a black-box test for the concurrent_tree's
// ability to rebalance in the face of many serial insertions.
// If the code survives many inserts, it is considered successful.
void concurrent_tree_unit_test::test_lkr_insert_serial_large(void) {
    comparator cmp;
    cmp.create(compare_dbts, nullptr);

    concurrent_tree tree;
    tree.create(&cmp);

    // prepare and acquire the infinte range
    concurrent_tree::locked_keyrange lkr;
    lkr.prepare(&tree);
    lkr.acquire(keyrange::get_infinite_range());

    // 128k keys should be fairly stressful.
    // a bad tree will flatten and die way earlier than 128k inserts.
    // a good tree will rebalance and reach height logn(128k) ~= 17,
    // survival the onslaught of inserts.
    const uint64_t num_keys = 128 * 1024;

    // populate the tree with all the keys
    for (uint64_t i = 0; i < num_keys; i++) {
        DBT k;
        toku_fill_dbt(&k, &i, sizeof(i));
        keyrange range;
        range.create(&k, &k);
        lkr.insert(range, i);
    }

    // remove all of the keys
    for (uint64_t i = 0; i < num_keys; i++) {
        DBT k;
        toku_fill_dbt(&k, &i, sizeof(i));
        keyrange range;
        range.create(&k, &k);
        lkr.remove(range);
    }

    lkr.release();
    tree.destroy();
}

} /* namespace toku */

int main(void) {
    toku::concurrent_tree_unit_test test;
    test.test_lkr_insert_serial_large();
    return 0;
}
