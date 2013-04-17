/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

static void
test_cachetable_def_flush (int n) {
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    const int test_limit = 2*n;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);
    assert_zero(r);
    char fname1[TOKU_PATH_MAX+1];
    unlink(toku_path_join(fname1, 2, TOKU_TEST_FILENAME, "test1.dat"));
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    char fname2[TOKU_PATH_MAX+1];
    unlink(toku_path_join(fname2, 2, TOKU_TEST_FILENAME, "test2.dat"));
    CACHEFILE f2;
    r = toku_cachetable_openf(&f2, ct, fname2, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    // insert keys 0..n-1 
    int i;
    for (i=0; i<n; i++) {
        uint32_t hi;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        toku_cachetable_put(f1, make_blocknum(i), hi, (void *)(long)i, make_pair_attr(1), wc, put_callback_nop);
        r = toku_test_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        toku_cachetable_put(f2, make_blocknum(i), hi, (void *)(long)i, make_pair_attr(1), wc, put_callback_nop);
        r = toku_test_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }
    toku_cachetable_verify(ct);

    // verify keys exists
    for (i=0; i<n; i++) {
        uint32_t hi;
        void *v;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(i), hi, PL_WRITE_EXPENSIVE, &v);
        assert(r == 0 && v == (void *)(long)i);
        r = toku_test_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f2, make_blocknum(i), hi, PL_WRITE_EXPENSIVE, &v);
        assert(r == 0 && v == (void *)(long)i);
        r = toku_test_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }

    // def_flush 
    toku_cachefile_flush(f1);
    toku_cachefile_verify(f1);

    // verify keys do not exist in f1 but do exist in f2
    for (i=0; i<n; i++) {
        uint32_t hi;
        void *v;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(i), hi, PL_WRITE_EXPENSIVE, &v);
        assert(r != 0);
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f2, make_blocknum(i), hi, PL_WRITE_EXPENSIVE, &v);
        assert(r == 0);
        r = toku_test_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }

    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachefile_close(&f2, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_cachetable_def_flush(8);
    return 0;
}
