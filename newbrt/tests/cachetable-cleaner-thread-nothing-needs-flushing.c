#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

//
// This test verifies that the cleaner thread doesn't call the callback if
// nothing needs flushing.
//

static UU() int
everything_pinned_cleaner_callback(
    void* UU(brtnode_pv),
    BLOCKNUM UU(blocknum),
    u_int32_t UU(fullhash),
    void* UU(extraargs)
    )
{
    assert(FALSE);  // everything is pinned so this should never be called
    return 0;
}

static void
run_test (void) {
    const int test_limit = 1000;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    r = toku_set_cleaner_period(ct, 1); assert(r == 0);

    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* vs[8];
    //void* v2;
    long ss[8];
    //long s2;
    for (int i = 0; i < 8; ++i) {
        r = toku_cachetable_get_and_pin(f1, make_blocknum(i+1), i+1, &vs[i], &ss[i],
                                        def_flush,
                                        def_fetch,
                                        def_pe_est_callback,
                                        def_pe_callback,
                                        def_pf_req_callback,
                                        def_pf_callback,
                                        everything_pinned_cleaner_callback,
                                        NULL, NULL);
        assert_zero(r);
        // set cachepressure_size to 0
        PAIR_ATTR attr = make_pair_attr(8);
        attr.cache_pressure_size = 0;
        r = toku_cachetable_unpin(f1, make_blocknum(i+1), i+1, CACHETABLE_CLEAN, attr);
        assert_zero(r);
    }

    usleep(4000000);

    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  run_test();
  return 0;
}
