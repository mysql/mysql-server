/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <stdio.h>
#include <sys/types.h>
#include <toku_assert.h>
#include <toku_time.h>

int main(void) {
    int r;
    struct timeval tv;
    struct timezone tz;

    r = gettimeofday(&tv, &tz);
    assert(r == 0);
    
    return 0;
}
