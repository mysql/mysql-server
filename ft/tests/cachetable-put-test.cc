/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "includes.h"
#include "test.h"

static void
cachetable_put_test (int n) {
    const int test_limit = 2*n;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    int i;
    for (i=1; i<=n; i++) {
        uint32_t hi;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
        toku_cachetable_put(f1, make_blocknum(i), hi, (void *)(long)i, make_pair_attr(1), wc, put_callback_nop);
        assert(toku_cachefile_count_pinned(f1, 0) == i);

        void *v;
        r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(i), hi, PL_WRITE_EXPENSIVE, &v);
        assert(r == -1);
        assert(toku_cachefile_count_pinned(f1, 0) == i);

        //r = toku_test_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, 1);
        //assert(r == 0);
        assert(toku_cachefile_count_pinned(f1, 0) == i);
    }
    for (i=n; i>0; i--) {
        uint32_t hi;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        r = toku_test_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
        assert(toku_cachefile_count_pinned(f1, 0) == i-1);
    }
    assert(toku_cachefile_count_pinned(f1, 1) == 0);
    toku_cachetable_verify(ct);

    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_put_test(8);
    return 0;
}
