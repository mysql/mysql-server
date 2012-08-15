/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

//
// simple tests for cleaner thread with an empty cachetable
//

static void
cachetable_test (void) {
    const int test_limit = 1000;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    r = toku_set_cleaner_period(ct, 1); assert(r == 0);

    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    usleep(4000000);
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_cachetable_begin_checkpoint(cp, NULL); assert(r == 0);
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
  cachetable_test();
  return 0;
}
