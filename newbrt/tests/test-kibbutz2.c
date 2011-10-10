/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: test-kibbutz.c 34445 2011-09-06 02:59:44Z zardosht $"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "kibbutz.h"
#include "includes.h"
#include "test.h"

#define ND 10
bool done[ND];

static void dowork (void *idv) {
    int *idp = idv;
    int id = *idp;
    if (verbose) printf("s%d\n", id);
    for (int i = 0; i < id; i++) {
        assert(done[i]);
    }
    assert(!done[id]);
    sleep(1);
    done[id] = true;
    sleep(1);
    if (verbose) printf("d%d\n", id);
}

static void kibbutz_test (void) {
    KIBBUTZ k = toku_kibbutz_create(1);
    if (verbose) printf("create\n");
    int ids[ND];
    for (int i=0; i<ND; i++) {
        done[i]=false;
        ids[i] =i;
    }
    for (int i=0; i<ND; i++) {
        if (verbose) printf("e%d\n", i);
        toku_kibbutz_enq(k, dowork, &ids[i]);
    }
    toku_kibbutz_destroy(k);
    for (int i=0; i<ND; i++) assert(done[i]);
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
    
    kibbutz_test();
    if (verbose) printf("test ok\n");
    return 0;
}


