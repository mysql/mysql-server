/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"
#include "logcursor.h"

int test_main(int argc, const char *argv[]) {
    int r;

    default_parse_args(argc, argv);

    toku_os_recursive_delete(TOKU_TEST_FILENAME);

    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);
    assert(r == 0);

    TOKULOGCURSOR lc;
    r = toku_logcursor_create(&lc, TOKU_TEST_FILENAME);
    assert(r == 0);

    toku_logcursor_print(lc);
    
    r = toku_logcursor_destroy(&lc);
    assert(r == 0);

    return 0;
}
