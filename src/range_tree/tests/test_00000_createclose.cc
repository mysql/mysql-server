/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* We are going to test whether we can create and close range trees */

#include "test.h"

static void test_create_close(bool allow_overlaps) {
    int r;
#ifdef TOKU_RT_NOOVERLAPS
    if (allow_overlaps) return;
#endif
    toku_range_tree *tree = NULL;
    r = toku_rt_create(&tree, int_cmp, char_cmp, allow_overlaps, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    
    assert(tree!=NULL);
    bool temp;
    r = toku_rt_get_allow_overlaps(tree, &temp);
    CKERR(r);
    assert((temp != 0) == (allow_overlaps != 0));
    
    r = toku_rt_close(tree);
    CKERR(r);
    tree = NULL;
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);

    test_create_close(false);
    test_create_close(true);
    
    return 0;
}

