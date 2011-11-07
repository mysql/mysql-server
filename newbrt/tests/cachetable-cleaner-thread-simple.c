#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

//
// This test verifies that the cleaner thread doesn't call the callback if
// nothing needs flushing.
//

CACHEFILE f1;
bool my_cleaner_callback_called;

static UU() int
my_cleaner_callback(
    void* UU(brtnode_pv),
    BLOCKNUM UU(blocknum),
    u_int32_t UU(fullhash),
    void* UU(extraargs)
    )
{
    assert(blocknum.b == 100);  // everything is pinned so this should never be called
    assert(fullhash == 100);
    PAIR_ATTR attr = make_pair_attr(8);
    attr.cache_pressure_size = 100;
    int r = toku_cachetable_unpin(f1, make_blocknum(100), 100, CACHETABLE_CLEAN, attr);
    my_cleaner_callback_called = true;
    return r;
}

static void
run_test (void) {
    const int test_limit = 1000;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    r = toku_set_cleaner_period(ct, 1); assert(r == 0);
    my_cleaner_callback_called = FALSE;

    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* vs[5];
    //void* v2;
    long ss[5];
    //long s2;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(100), 100, &vs[4], &ss[4],
                                    def_flush,
                                    def_fetch,
                                    def_pe_est_callback,
                                    def_pe_callback,
                                    def_pf_req_callback,
                                    def_pf_callback,
                                    my_cleaner_callback,
                                    NULL, NULL);
    PAIR_ATTR attr = make_pair_attr(8);
    attr.cache_pressure_size = 100;
    r = toku_cachetable_unpin(f1, make_blocknum(100), 100, CACHETABLE_CLEAN, attr);

    for (int i = 0; i < 4; ++i) {
        r = toku_cachetable_get_and_pin(f1, make_blocknum(i+1), i+1, &vs[i], &ss[i],
                                        def_flush,
                                        def_fetch,
                                        def_pe_est_callback,
                                        def_pe_callback,
                                        def_pf_req_callback,
                                        def_pf_callback,
                                        def_cleaner_callback,
                                        NULL, NULL);
        assert_zero(r);
        // set cachepressure_size to 0
        attr = make_pair_attr(8);
        attr.cache_pressure_size = 0;
        r = toku_cachetable_unpin(f1, make_blocknum(i+1), i+1, CACHETABLE_CLEAN, attr);
        assert_zero(r);
    }

    usleep(4000000);
    assert(my_cleaner_callback_called);

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
