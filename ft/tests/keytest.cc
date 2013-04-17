/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"
#include "key.h"

void
toku_test_keycompare (void) {
    assert(toku_keycompare("a",1, "a",1)==0);
    assert(toku_keycompare("aa",2, "a",1)>0);
    assert(toku_keycompare("a",1, "aa",2)<0);
    assert(toku_keycompare("b",1, "aa",2)>0);
    assert(toku_keycompare("aa",2, "b",1)<0);
    assert(toku_keycompare("aaaba",5, "aaaba",5)==0);
    assert(toku_keycompare("aaaba",5, "aaaaa",5)>0);
    assert(toku_keycompare("aaaaa",5, "aaaba",5)<0);
    assert(toku_keycompare("aaaaa",3, "aaaba",3)==0);
    assert(toku_keycompare("\000\000\000\a", 4, "\000\000\000\004", 4)>0);
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    toku_test_keycompare();
    if (verbose) printf("test ok\n");
    return 0;
}
