#include "includes.h"
#include "test.h"

static void
flush (CACHEFILE cf     __attribute__((__unused__)),
       CACHEKEY key     __attribute__((__unused__)),
       void *v          __attribute__((__unused__)),
       void *extraargs  __attribute__((__unused__)),
       long size        __attribute__((__unused__)),
       BOOL write_me    __attribute__((__unused__)),
       BOOL keep_me     __attribute__((__unused__)),
       BOOL for_checkpoint    __attribute__((__unused__))
       ) {
    assert((long) key.b == size);
    if (!keep_me) toku_free(v);
}

static int
fetch (CACHEFILE cf, CACHEKEY key, u_int32_t hash, void **vptr, long *sizep, void *extra) {
    cf = cf; hash = hash; extra = extra;
    *sizep = (long) key.b;
    *vptr = toku_malloc(*sizep);
    return 0;
}

static int
fetch_error (CACHEFILE cf       __attribute__((__unused__)),
	     CACHEKEY key       __attribute__((__unused__)),
	     u_int32_t fullhash __attribute__((__unused__)),
	     void **value       __attribute__((__unused__)),
	     long *sizep        __attribute__((__unused__)),
	     void*extraargs     __attribute__((__unused__))
	     ) {
    return -1;
}

static void
cachetable_getandpin_test (int n) {
    const int test_limit = 1024*1024;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test_getandpin.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    int i;

    // test get_and_pin fails
    for (i=1; i<=n; i++) {
        u_int32_t hi;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        void *v; long size;
        r = toku_cachetable_get_and_pin(f1, make_blocknum(i), hi, &v, &size, flush, fetch_error, 0);
        assert(r == -1);
    }

    // test get_and_pin size
    for (i=1; i<=n; i++) {
        u_int32_t hi;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        void *v; long size;
        r = toku_cachetable_get_and_pin(f1, make_blocknum(i), hi, &v, &size, flush, fetch, 0);
        assert(r == 0);
        assert(size == i);

        r = toku_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, i);
        assert(r == 0);
    }
    toku_cachetable_verify(ct);

    r = toku_cachefile_close(&f1, NULL_LOGGER, 0, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_getandpin_test(8);
    return 0;
}
