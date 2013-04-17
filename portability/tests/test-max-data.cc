/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <stdio.h>
#include <toku_stdint.h>
#include <unistd.h>
#include <toku_assert.h>
#include <string.h>
#include "toku_os.h"

int main(int argc, char *const argv[]) {
    int verbose = 0;
    int i;
    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            verbose = 0;
            continue;
        }
    }
        
    // get the data size
    uint64_t maxdata;
    int r = toku_os_get_max_process_data_size(&maxdata);
    assert(r == 0);
    if (verbose) printf("maxdata=%" PRIu64 " 0x%" PRIx64 "\n", maxdata, maxdata);

    // check the data size
#if __x86_64__
    assert(maxdata > (1ULL << 32));
#elif __i386__
    assert(maxdata < (1ULL << 32));
#else
    #error
#endif

    return 0;
}
