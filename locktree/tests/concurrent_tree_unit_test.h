/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

#include <concurrent_tree.h>

namespace toku {

class concurrent_tree_unit_test {
public:
    // creating a concurrent tree should initialize it to a valid,
    // empty state. the root node should be properly marked, have
    // no children, and the correct comparator.
    void test_create_destroy(void);

    // acquiring a locked keyrange should lock and "root" itself at
    // the proper subtree node. releasing it should unlock that node.
    void test_lkr_acquire_release(void);

    // remove_all on a locked keyrange should properly remove everything
    // from the tree and account correctly for the amount of memory released.
    void test_lkr_remove_all(void);

    // test that insert/remove work properly together, confirming
    // whether keys exist using iterate()
    void test_lkr_insert_remove(void);

    // test that the concurrent tree can survive many serial inserts
    // this is a blackbox test for tree rotations.
    void test_lkr_insert_serial_large(void);

private:

    // populate the given concurrent tree with elements from min..max but
    // starting with a certain element. this allows the caller to modestly
    // control the way the tree is built/rotated, for test variability.
    static void populate_tree(concurrent_tree *tree, uint64_t start, uint64_t min, uint64_t max) {
        concurrent_tree::locked_keyrange lkr;
        lkr.prepare(tree);
        lkr.acquire(keyrange::get_infinite_range());

        for (uint64_t i = start; i <= max; i++) {
            keyrange range;
            range.create(get_dbt(i), get_dbt(i));
            lkr.insert(range, i);
        }
        for (uint64_t i = min; i < start; i++) {
            keyrange range;
            range.create(get_dbt(i), get_dbt(i));
            lkr.insert(range, i);
        }

        lkr.release();
    }
};

} /* namespace toku */
