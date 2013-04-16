/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <stdio.h>
#include <toku_assert.h>
#include <toku_stdint.h>
#include <toku_os.h>

int verbose = 0;

int main(void) {
    uint64_t cpuhz;
    int r = toku_os_get_processor_frequency(&cpuhz);
    assert(r == 0);
    if (verbose) {
	printf("%" PRIu64 "\n", cpuhz);
    }
    assert(cpuhz>100000000);
    return 0;
}
