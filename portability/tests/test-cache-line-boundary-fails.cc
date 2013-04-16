/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <config.h>
#include <memory.h>
#include <portability/toku_atomic.h>
#include "test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int verbose = 0;

static const size_t cachelinesize = 64;

// cache line is 64 bytes
// nine 7-byte structs fill 63 bytes
// the tenth spans one byte of the first cache line and six of the next cache line
// we first SFAA the first 9 structs and ensure we don't crash, then we set a signal handler and SFAA the 10th and ensure we do crash

struct unpackedsevenbytestruct {
    uint32_t i;
    char pad[3];
};
struct __attribute__((packed)) packedsevenbytestruct {
    uint32_t i;
    char pad[3];
};

struct packedsevenbytestruct *psevenbytestructs;
static __attribute__((__noreturn__)) void catch_abort (int sig __attribute__((__unused__))) {
    toku_free(psevenbytestructs);
#ifdef TOKU_DEBUG_PARANOID
    exit(EXIT_SUCCESS);  // with paranoid asserts, we expect to assert and reach this handler
#else
    exit(EXIT_FAILURE);  // we should not have crashed without paranoid asserts
#endif
}

int test_main(int UU(argc), char *const argv[] UU()) {
    if (sizeof(unpackedsevenbytestruct) != 8) {
        exit(EXIT_FAILURE);
    }
    if (sizeof(packedsevenbytestruct) != 7) {
        exit(EXIT_FAILURE);
    }

    {
        struct unpackedsevenbytestruct *MALLOC_N_ALIGNED(cachelinesize, 10, usevenbytestructs);
        if (usevenbytestructs == NULL) {
            // this test is supposed to crash, so exiting cleanly is a failure
            perror("posix_memalign");
            exit(EXIT_FAILURE);
        }

        for (int idx = 0; idx < 10; ++idx) {
            usevenbytestructs[idx].i = idx + 1;
            (void) toku_sync_fetch_and_add(&usevenbytestructs[idx].i, 32U - idx);
        }
        toku_free(usevenbytestructs);
    }

    
    MALLOC_N_ALIGNED(cachelinesize, 10, psevenbytestructs);
    if (psevenbytestructs == NULL) {
        // this test is supposed to crash, so exiting cleanly is a failure
        perror("posix_memalign");
        exit(EXIT_FAILURE);
    }

    for (int idx = 0; idx < 9; ++idx) {
        psevenbytestructs[idx].i = idx + 1;
        (void) toku_sync_fetch_and_add(&psevenbytestructs[idx].i, 32U - idx);
    }
    psevenbytestructs[9].i = 10;
    signal(SIGABRT, catch_abort);
    (void) toku_sync_fetch_and_add(&psevenbytestructs[9].i, 32U);

#ifdef TOKU_DEBUG_PARANOID
    exit(EXIT_FAILURE);  // with paranoid asserts, we should already have crashed
#else
    exit(EXIT_SUCCESS);  // without them, we should make it here
#endif
}
