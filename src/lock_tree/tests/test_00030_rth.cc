/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* We are going to test whether create and close properly check their input. */

#include "test.h"

toku_rth* rth;

int main(int argc, const char *argv[]) {
    int r;
    parse_args(argc, argv);
    

    size_t i;
    size_t iterations = 512 << 2;
    
    r = toku_rth_create(&rth);
    CKERR(r);
    assert(rth);
    for (i = 1; i < iterations; i++) {
        r = toku_rth_insert(rth, (TXNID)i);
        CKERR(r);
    }
    rt_forest* f;
    for (i = 1; i < iterations; i++) {
        f = toku_rth_find(rth, (TXNID)i);
        assert(f);
    }
    f = toku_rth_find(rth, (TXNID)i);
    assert(!f);
    for (i = iterations - 1; i >= 1; i--) {
        toku_rth_delete(rth, (TXNID)i);
    }
    toku_rth_close(rth);
    rth = NULL;

    /* ********************************************************************** */

    r = toku_rth_create(&rth);
    CKERR(r);
    assert(rth);
    for (i = 1; i < iterations; i++) {
        r = toku_rth_insert(rth, (TXNID)i);
        CKERR(r);
    }
    for (i = 1; i < iterations; i++) {
        toku_rth_delete(rth, (TXNID)i);
    }
    toku_rth_close(rth);
    rth = NULL;

    /* ********************************************************************** */

    r = toku_rth_create(&rth);
    CKERR(r);
    assert(rth);
    for (i = iterations - 1; i >= 1; i--) {
        r = toku_rth_insert(rth, (TXNID)i);
        CKERR(r);
    }
    toku_rth_close(rth);
    rth = NULL;

    /* ********************************************************************** */

    r = toku_rth_create(&rth);
    CKERR(r);
    assert(rth);
    for (i = iterations - 1; i >= 1; i--) {
        r = toku_rth_insert(rth, (TXNID)i);
        CKERR(r);
    }
    toku_rth_clear(rth);
    assert(toku_rth_is_empty(rth));
    toku_rth_close(rth);
    rth = NULL;

    return 0;
}
