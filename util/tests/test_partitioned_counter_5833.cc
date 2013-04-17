/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// Demonstrate a race if #5833 isn't fixed.

#include <pthread.h>
#include <toku_portability.h>
#include <util/partitioned_counter.h>
#include "test.h"


static void pt_create (pthread_t *thread, void *(*f)(void*), void *extra) {
    int r = pthread_create(thread, NULL, f, extra);
    assert(r==0);
}

static void pt_join (pthread_t thread, void *expect_extra) {
    void *result;
    int r = pthread_join(thread, &result);
    assert(r==0);
    assert(result==expect_extra);
}

static int verboseness_cmdarg=0;

static void parse_args (int argc, const char *argv[]) {
    const char *progname = argv[1];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v")==0) verboseness_cmdarg++;
	else {
	    printf("Usage: %s [-v]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
}

#define NCOUNTERS 2
PARTITIONED_COUNTER array_of_counters[NCOUNTERS];

static void *counter_init_fun(void *tnum_pv) {
    int *tnum_p = (int*)tnum_pv;
    int tnum = *tnum_p;
    assert(0<=tnum  && tnum<NCOUNTERS);
    array_of_counters[tnum] = create_partitioned_counter();
    return tnum_pv;
}

static void do_test_5833(void) {
    pthread_t threads[NCOUNTERS];
    int       tids[NCOUNTERS];
    for (int i=0; i<NCOUNTERS; i++) {
        tids[i] = i;
        pt_create(&threads[i], counter_init_fun, &tids[i]);
    }
    for (int i=0; i<NCOUNTERS; i++) {
        pt_join(threads[i], &tids[i]);
        destroy_partitioned_counter(array_of_counters[i]);
    }
}

int test_main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    do_test_5833();
    return 0;
}
