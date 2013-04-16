/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "concurrent_tree_unit_test.h"

namespace toku {

// remove_all on a locked keyrange should properly remove everything
// from the tree and account correctly for the amount of memory released.
void concurrent_tree_unit_test::test_lkr_remove_all(void) {
    comparator cmp;
    cmp.create(compare_dbts, nullptr);

    // we'll test a tree that has values 0..20
    const uint64_t min = 0;
    const uint64_t max = 20;

    // remove_all should work regardless of how the
    // data was inserted into the tree, so we test it
    // on a tree whose elements were populated starting
    // at each value 0..20 (so we get different rotation
    // behavior for each starting value in the tree).
    for (uint64_t start = min; start <= max; start++) {
        concurrent_tree tree;
        concurrent_tree::locked_keyrange lkr;

        tree.create(&cmp);
        populate_tree(&tree, start, min, max);
        invariant(!tree.is_empty());

        lkr.prepare(&tree);
        invariant(lkr.m_subtree->is_root());
        invariant(!lkr.m_subtree->is_empty());

        // remove_all() from the locked keyrange and assert that
        // the number of elements and memory removed is correct.
        lkr.remove_all();

        invariant(lkr.m_subtree->is_empty());
        invariant(tree.is_empty());
        invariant_null(tree.m_root.m_right_child.ptr);
        invariant_null(tree.m_root.m_left_child.ptr);

        lkr.release();
        tree.destroy();
    }
}

} /* namespace toku */

int main(void) {
    toku::concurrent_tree_unit_test test;
    test.test_lkr_remove_all();
    return 0;
}
