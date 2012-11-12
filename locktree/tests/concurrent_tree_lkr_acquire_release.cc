/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "concurrent_tree_unit_test.h"

namespace toku {

void concurrent_tree_unit_test::test_lkr_acquire_release(void) {
    comparator cmp;
    cmp.create(compare_dbts, nullptr);

    // we'll test a tree that has values 0..20
    const uint64_t min = 0;
    const uint64_t max = 20;

    // acquire/release should work regardless of how the
    // data was inserted into the tree, so we test it
    // on a tree whose elements were populated starting
    // at each value 0..20 (so we get different rotation
    // behavior for each starting value in the tree).
    for (uint64_t start = min; start <= max; start++) {
        concurrent_tree tree;
        tree.create(&cmp);
        populate_tree(&tree, start, min, max);
        invariant(!tree.is_empty());

        for (uint64_t i = 0; i <= max; i++) {
            concurrent_tree::locked_keyrange lkr;
            lkr.prepare(&tree);
            invariant(lkr.m_tree == &tree);
            invariant(lkr.m_subtree->is_root());

            keyrange range;
            range.create(get_dbt(i), get_dbt(i));
            lkr.acquire(range);
            // the tree is not empty so the subtree root should not be empty
            invariant(!lkr.m_subtree->is_empty());

            // if the subtree root does not overlap then one of its children
            // must exist and have an overlapping range.
            if (!lkr.m_subtree->m_range.overlaps(&cmp, range)) {
                treenode *left = lkr.m_subtree->m_left_child.ptr;
                treenode *right = lkr.m_subtree->m_right_child.ptr;
                if (left != nullptr) {
                    // left exists, so if it does not overlap then the right must
                    if (!left->m_range.overlaps(&cmp, range)) {
                        invariant_notnull(right);
                        invariant(right->m_range.overlaps(&cmp, range));
                    }
                } else {
                    // no left child, so the right must exist and be overlapping
                    invariant_notnull(right);
                    invariant(right->m_range.overlaps(&cmp, range));
                }
            }

            lkr.release();
        }

        // remove everything one by one and then destroy
        keyrange range;
        concurrent_tree::locked_keyrange lkr;
        lkr.prepare(&tree);
        invariant(lkr.m_subtree->is_root());
        range.create(get_dbt(min), get_dbt(max));
        lkr.acquire(range);
        invariant(lkr.m_subtree->is_root());
        for (uint64_t i = 0; i <= max; i++) {
            range.create(get_dbt(i), get_dbt(i));
            lkr.remove(range);
        }
        lkr.release();
        tree.destroy();
    }
}

} /* namespace toku */

int main(void) {
    toku::concurrent_tree_unit_test test;
    test.test_lkr_acquire_release();
    return 0;
}
