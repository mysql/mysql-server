/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* We are going to test whether close can clean up after itself. */

#include "test.h"

static void
run_test (bool overlap_allowed) {
    int r;
    toku_range_tree *tree;
    toku_range range;
    int nums[8] = {0,1,2,3,4,5,6,7};
    char letters[2] = {'A','B'};



    /* Test overlap case */
    /*
        1   2   3   4   5   6   7
        |---A-----------|
    */
    r = toku_rt_create(&tree, int_cmp, char_cmp, overlap_allowed, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);

    range.ends.left = (toku_point*)&nums[1];
    range.ends.right = (toku_point*)&nums[5];
    range.data = (TXNID)letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_close(tree);            CKERR(r);

    tree = NULL;

}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

#ifndef TOKU_RT_NOOVERLAPS
    run_test(true);
#endif
    run_test(false);
    return 0;
}
