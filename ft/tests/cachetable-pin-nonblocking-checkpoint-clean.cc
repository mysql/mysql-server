/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"
#include "cachetable-test.h"

CACHETABLE ct;

CACHEFILE f1;

static void
run_test (void) {
    const int test_limit = 20;
    int r;
    ct = NULL;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    f1 = NULL;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    create_dummy_functions(f1);

    void* v1;
    void* v2;
    long s1;
    long s2;
    
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, def_write_callback(NULL), def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8)); assert(r==0);

    for (int i = 0; i < 20; i++) {
        r = toku_cachetable_get_and_pin(f1, make_blocknum(2), 2, &v2, &s2, def_write_callback(NULL), def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
        r = toku_test_cachetable_unpin(f1, make_blocknum(2), 2, CACHETABLE_CLEAN, make_pair_attr(8)); assert(r==0);
    }

    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v2, &s2, def_write_callback(NULL), def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_cachetable_begin_checkpoint(cp, NULL);
    // mark nodes as pending a checkpoint, so that get_and_pin_nonblocking on block 1 will return TOKUDB_TRY_AGAIN
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8)); assert(r==0);

    r = toku_cachetable_get_and_pin_nonblocking(
        f1,
        make_blocknum(1),
        1,
        &v1,
        &s1,
        def_write_callback(NULL),
        def_fetch,
        def_pf_req_callback,
        def_pf_callback,
        PL_WRITE_EXPENSIVE,
        NULL,
        NULL
        );
    assert(r==0);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8)); assert(r==0);
    
    r = toku_cachetable_end_checkpoint(
        cp, 
        NULL, 
        NULL,
        NULL
        );
    assert(r==0);
    
    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, 0, false, ZERO_LSN); assert(r == 0);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);
    
    
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  run_test();
  return 0;
}
