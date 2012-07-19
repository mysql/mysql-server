/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"
#include "logfilemgr.h"

int test_main(int argc __attribute__((unused)), const char *argv[] __attribute__((unused))) {
    int r;

    TOKULOGFILEMGR lfm = NULL;
    r = toku_logfilemgr_create(&lfm);
    assert(r == 0);

    toku_logfilemgr_print(lfm);

    r = toku_logfilemgr_destroy(&lfm);
    assert(r == 0);

    return 0;
}
