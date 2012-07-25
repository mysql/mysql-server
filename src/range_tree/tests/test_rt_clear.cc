/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// test that the toku_rt_clear function works

#include "test.h"

static int count_range_callback(toku_range *range UU(), void *extra) {
    int *counter = (int *) extra;
    *counter += 1;
    return 0;
}

static int count_ranges(toku_range_tree *tree) {
    int counter = 0;
    int r = toku_rt_iterate(tree, count_range_callback, &counter); CKERR(r);
    return counter;
}

static void my_init_range(toku_range *range, int *left, int *right, int data) {
    range->ends.left = (toku_point *) left;
    range->ends.right = (toku_point *) right;
    range->data = data;
}

int main(int argc, const char *argv[]) {
    int r;

    parse_args(argc, argv);

    toku_range_tree *tree;
    r = toku_rt_create(&tree, int_cmp, char_cmp, false, test_incr_memory_size, test_decr_memory_size, NULL); 
    CKERR(r);
    assert(count_ranges(tree) == 0);

    const int nranges = 10;
    int nums[nranges];
    for (int i = 0; i < nranges; i++) {
        assert(count_ranges(tree) == i);
        uint32_t treesize = toku_rt_get_size(tree);
        assert(treesize == (uint32_t) i);
        nums[i] = i;
        toku_range range; my_init_range(&range, &nums[i], &nums[i], 'a');
        r = toku_rt_insert(tree, &range); CKERR(r);
    }

    assert(count_ranges(tree) == nranges);
    toku_rt_clear(tree);
    assert(count_ranges(tree) == 0);

    r = toku_rt_close(tree); CKERR(r);

    return 0;
}
