/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"
#include "cachetable-test.h"

//
// simple tests for maybe_get_and_pin(_clean)
//

static void
cachetable_test (void) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    create_dummy_functions(f1);
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    
    void* v1;
    long s1;
    // nothing in cachetable, so this should fail
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

    // maybe_get_and_pin_clean should succeed, maybe_get_and_pin should fail
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    r = toku_cachetable_maybe_get_and_pin_clean(f1, make_blocknum(1), 1, &v1);
    assert(r == 0);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));
    // maybe_get_and_pin_clean should succeed, maybe_get_and_pin should fail
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==0);
    // now these calls should fail because the node is already pinned, and therefore in use
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    r = toku_cachetable_maybe_get_and_pin_clean(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));

    // sanity check, this should still succeed, because the PAIR is dirty
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==0);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    toku_cachetable_begin_checkpoint(cp, NULL);
    // now these should fail, because the node should be pending a checkpoint
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    toku_cachetable_end_checkpoint(
        cp, 
        NULL, 
        NULL,
        NULL
        );

    toku_cachetable_verify(ct);
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test();
  return 0;
}
