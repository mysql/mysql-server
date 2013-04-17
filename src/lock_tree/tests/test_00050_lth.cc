/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* We are going to test whether create and close properly check their input. */

#include "test.h"

toku_lth* lth;

int main(int argc, const char *argv[]) {
    int r;
    parse_args(argc, argv);
    
    lth = NULL;
    r = toku_lth_create(&lth);
    CKERR(r);
    assert(lth);
    toku_lth_close(lth);
    lth = NULL;

    size_t i;
    size_t iterations = 512 << 2;
    
    r = toku_lth_create(&lth);
    CKERR(r);
    assert(lth);
    for (i = 1; i < iterations; i++) {
        r = toku_lth_insert(lth, (toku_lock_tree*)i);
        CKERR(r);
    }
    toku_lock_tree* f;
    for (i = 1; i < iterations; i++) {
        f = toku_lth_find(lth, (toku_lock_tree*)i);
        assert(f == (toku_lock_tree*) i);
    }
    f = toku_lth_find(lth, (toku_lock_tree*)i);
    assert(!f);
    
    toku_lth_start_scan(lth);
    for (i = 1; i < iterations; i++) {
        f = toku_lth_next(lth);
        assert(f);
    }
    f = toku_lth_next(lth);
    assert(!f);

    for (i = iterations - 1; i >= 1; i--) {
        toku_lth_delete(lth, (toku_lock_tree*)i);
    }
    toku_lth_close(lth);
    lth = NULL;

    r = toku_lth_create(&lth);
    CKERR(r);
    assert(lth);
    for (i = 1; i < iterations; i++) {
        r = toku_lth_insert(lth, (toku_lock_tree*)i);
        CKERR(r);
    }
    for (i = 1; i < iterations; i++) {
        toku_lth_delete(lth, (toku_lock_tree*)i);
    }
    toku_lth_close(lth);
    lth = NULL;


    r = toku_lth_create(&lth);
    CKERR(r);
    assert(lth);
    for (i = iterations - 1; i >= 1; i--) {
        r = toku_lth_insert(lth, (toku_lock_tree*)i);
        CKERR(r);
    }
    toku_lth_close(lth);
    lth = NULL;

    return 0;
}
