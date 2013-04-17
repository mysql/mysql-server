#ident "$Id$"
#include "includes.h"
#include "test.h"

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void *e     __attribute__((__unused__)),
       long s      __attribute__((__unused__)),
        long* new_size      __attribute__((__unused__)),
       BOOL w      __attribute__((__unused__)),
       BOOL keep   __attribute__((__unused__)),
       BOOL c      __attribute__((__unused__))
       ) {
    /* Do nothing */
}

static void 
pe_est_callback(
    void* UU(brtnode_pv), 
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    *bytes_freed_estimate = 0;
    *cost = PE_CHEAP;
}

static int 
pe_callback (
    void *brtnode_pv __attribute__((__unused__)), 
    long bytes_to_free __attribute__((__unused__)), 
    long* bytes_freed, 
    void* extraargs __attribute__((__unused__))
    ) 
{
    *bytes_freed = bytes_to_free;
    return 0;
}

static void
test_cachetable_flush (int n) {
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
        r = toku_cachetable_put(f1, make_blocknum(i), hi, (void *)(long)i, 1, flush, pe_est_callback, pe_callback, 0);
        assert(r == 0);
        r = toku_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        r = toku_cachetable_put(f2, make_blocknum(i), hi, (void *)(long)i, 1, flush, pe_est_callback, pe_callback, 0);
        assert(r == 0);
        r = toku_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, 1);
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
        r = toku_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f2, make_blocknum(i), hi, &v);
        assert(r == 0 && v == (void *)(long)i);
        r = toku_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
    }

    // flush 
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
        r = toku_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
    }

    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachefile_close(&f2, 0, FALSE, ZERO_LSN); assert(r == 0 && f2 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_cachetable_flush(8);
    return 0;
}
