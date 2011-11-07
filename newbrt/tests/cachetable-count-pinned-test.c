#ident "$Id$"
#include "includes.h"
#include "test.h"


static void
cachetable_count_pinned_test (int n) {
    const int test_limit = 2*n;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    int i;
    for (i=1; i<=n; i++) {
        u_int32_t hi;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        r = toku_cachetable_put(f1, make_blocknum(i), hi, (void *)(long)i, make_pair_attr(1), def_flush, def_pe_est_callback, def_pe_callback, def_cleaner_callback, 0);
        assert(r == 0);
        assert(toku_cachefile_count_pinned(f1, 0) == i);

        void *v;
        r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(i), hi, &v);
        assert(r == -1);
        assert(toku_cachefile_count_pinned(f1, 0) == i);

        //r = toku_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, 1);
        //assert(r == 0);
        assert(toku_cachefile_count_pinned(f1, 0) == i);
    }
    for (i=n; i>0; i--) {
        u_int32_t hi;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        r = toku_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
        if (i-1) assert(toku_cachetable_assert_all_unpinned(ct));
        assert(toku_cachefile_count_pinned(f1, 0) == i-1);
    }
    assert(toku_cachetable_assert_all_unpinned(ct) == 0);
    assert(toku_cachefile_count_pinned(f1, 1) == 0);
    toku_cachetable_verify(ct);

    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_count_pinned_test(8);
    return 0;
}
