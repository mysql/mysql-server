/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "concurrent_tree_unit_test.h"

namespace toku {

static comparator cmp;

// test that creating a concurrent tree puts it in a valid, empty state.
// the root node should be properly marked and have the correct comparator.
void concurrent_tree_unit_test::test_create_destroy(void) {
    concurrent_tree tree;
    tree.create(&cmp);

    invariant(tree.m_root.is_root());
    invariant(tree.m_root.is_empty());
    invariant(tree.m_root.m_cmp == &cmp);
    invariant_null(tree.m_root.m_left_child.ptr);
    invariant_null(tree.m_root.m_right_child.ptr);

    invariant(tree.is_empty());

    tree.destroy();
}

} /* namespace toku */

int main(void) {
    toku::concurrent_tree_unit_test test;
    test.test_create_destroy();
    return 0;
}
