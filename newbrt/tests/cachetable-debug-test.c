#include "includes.h"
#include "test.h"

static void
flush (CACHEFILE f __attribute__((__unused__)),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void *e     __attribute__((__unused__)),
       long s      __attribute__((__unused__)),
       BOOL w      __attribute__((__unused__)),
       BOOL keep   __attribute__((__unused__)),
       LSN m       __attribute__((__unused__)),
       BOOL r      __attribute__((__unused__))
       ) {
    /* Do nothing */
}

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       CACHEKEY k         __attribute__((__unused__)),
       u_int32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       long *sizep        __attribute__((__unused__)),
       void *extraargs    __attribute__((__unused__)),
       LSN *written_lsn    __attribute__((__unused__))
       ) {
    return 0;
}

static void
cachetable_debug_test (int n) {
    const int test_limit = n;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    int num_entries, hash_size; long size_current, size_limit;
    toku_cachetable_get_state(ct, &num_entries, &hash_size, &size_current, &size_limit);
    assert(num_entries == 0);
    assert(size_current == 0);
    assert(size_limit == n);
    // printf("%d %d %ld %ld\n", num_entries, hash_size, size_current, size_limit);

    int i;
    for (i=1; i<=n; i++) {
        const int item_size = 1;
        u_int32_t hi;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        r = toku_cachetable_put(f1, make_blocknum(i), hi, (void *)(long)i, item_size, flush, fetch, 0);
        assert(r == 0);

        void *v; int dirty; long long pinned; long pair_size;
        r = toku_cachetable_get_key_state(ct, make_blocknum(i), f1, &v, &dirty, &pinned, &pair_size);
        assert(r == 0);
        assert(v == (void *)(long)i);
        assert(dirty == CACHETABLE_DIRTY);
        assert(pinned == 1);
        assert(pair_size == item_size);

        r = toku_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);

        toku_cachetable_get_state(ct, &num_entries, &hash_size, &size_current, &size_limit);
        assert(num_entries == i);
        assert(size_current == i);
        assert(size_limit == n);

        toku_cachetable_print_state(ct);
    }
    toku_cachetable_verify(ct);

    print_hash_histogram();

    r = toku_cachefile_close(&f1, NULL_LOGGER); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_debug_test(8);
    return 0;
}
