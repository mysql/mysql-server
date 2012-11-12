/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "test.h"


static void
cachetable_test (void) {
    int num_entries = 100;
    int test_limit = 6;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    // test that putting something too big in the cachetable works fine
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    toku_cachetable_put(f1, make_blocknum(num_entries+1), num_entries+1, NULL, make_pair_attr(test_limit*2), wc, put_callback_nop);
    r = toku_test_cachetable_unpin(f1, make_blocknum(num_entries+1), num_entries+1, CACHETABLE_DIRTY, make_pair_attr(test_limit*2));
    assert(r==0);


    for (int64_t i = 0; i < num_entries; i++) {
        toku_cachetable_put(f1, make_blocknum(i), i, NULL, make_pair_attr(1), wc, put_callback_nop);
        assert(toku_cachefile_count_pinned(f1, 0) == (i+1));
    }
    for (int64_t i = 0; i < num_entries; i++) {
        r = toku_test_cachetable_unpin(f1, make_blocknum(i), i, CACHETABLE_DIRTY, make_pair_attr(1));
    }

    
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_test();
    return 0;
}
