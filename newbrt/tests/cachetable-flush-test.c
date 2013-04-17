#ident "$Id$"
#include "includes.h"
#include "test.h"

static void
test_cachetable_def_flush (int n) {
    const int test_limit = 2*n;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    char fname2[] = __FILE__ "test2.dat";
    unlink(fname2);
    CACHEFILE f2;
    r = toku_cachetable_openf(&f2, ct, fname2, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    // insert keys 0..n-1 
    int i;
    for (i=0; i<n; i++) {
        u_int32_t hi;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        r = toku_cachetable_put(f1, make_blocknum(i), hi, (void *)(long)i, make_pair_attr(1), def_flush, def_pe_est_callback, def_pe_callback, def_cleaner_callback, 0);
        assert(r == 0);
        r = toku_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        r = toku_cachetable_put(f2, make_blocknum(i), hi, (void *)(long)i, make_pair_attr(1), def_flush, def_pe_est_callback, def_pe_callback, def_cleaner_callback, 0);
        assert(r == 0);
        r = toku_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }
    toku_cachetable_verify(ct);

    // verify keys exists
    for (i=0; i<n; i++) {
        u_int32_t hi;
        void *v;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(i), hi, &v);
        assert(r == 0 && v == (void *)(long)i);
        r = toku_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f2, make_blocknum(i), hi, &v);
        assert(r == 0 && v == (void *)(long)i);
        r = toku_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }

    // def_flush 
    r = toku_cachefile_flush(f1); assert(r == 0);
    toku_cachefile_verify(f1);

    // verify keys do not exist in f1 but do exist in f2
    for (i=0; i<n; i++) {
        u_int32_t hi;
        void *v;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(i), hi, &v);
        assert(r != 0);
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f2, make_blocknum(i), hi, &v);
        assert(r == 0);
        r = toku_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }

    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachefile_close(&f2, 0, FALSE, ZERO_LSN); assert(r == 0 && f2 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_cachetable_def_flush(8);
    return 0;
}
