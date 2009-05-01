/* -*- mode: C; c-basic-offset: 4 -*- */

/* verify that maybe_get_and_pin returns an error while a prefetch block is pending */
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
       BOOL c      __attribute__((__unused__))
       ) {
    assert(w == FALSE);
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

    sleep(10);

    *value = 0;
    *sizep = 1;
    *written_lsn = ZERO_LSN;

    return 0;
}

static void cachetable_prefetch_maybegetandpin_test (void) {
    const int test_limit = 1;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    // prefetch block 0. this will take 10 seconds.
    CACHEKEY key = make_blocknum(0);
    u_int32_t fullhash = toku_cachetable_hash(f1, make_blocknum(0));
    r = toku_cachefile_prefetch(f1, key, fullhash, flush, fetch, 0);
    toku_cachetable_verify(ct);

    // verify that maybe_get_and_pin returns an error while the prefetch is in progress
    int i;
    for (i=1; i>=0; i++) {
        void *v;
        r = toku_cachetable_maybe_get_and_pin_clean(f1, key, fullhash, &v);
        if (r == 0) break;
        toku_pthread_yield();
    }
    if (verbose) printf("%s:%d %d\n", __FUNCTION__, __LINE__, i);
    assert(i>1);
    toku_cachetable_verify(ct);

    r = toku_cachetable_unpin(f1, key, fullhash, CACHETABLE_CLEAN, 1);
    assert(r == 0);
    toku_cachetable_verify(ct);

    r = toku_cachefile_close(&f1, NULL_LOGGER, 0); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_prefetch_maybegetandpin_test();
    return 0;
}
