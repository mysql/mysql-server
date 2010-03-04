/* -*- mode: C; c-basic-offset: 4 -*- */

// verify that cachetable prefetch multiple blocks hits the cachetable size limit
// and flushes eventually happen.

#include "includes.h"
#include "test.h"

static int flush_calls = 0;
static int flush_evict_calls = 0;
static int evicted_keys = 0;

static void
flush (CACHEFILE f __attribute__((__unused__)),
       CACHEKEY k,
       void *v     __attribute__((__unused__)),
       void *e     __attribute__((__unused__)),
       long s      __attribute__((__unused__)),
       BOOL w,
       BOOL keep,
       BOOL f_ckpt __attribute__((__unused__))
       ) {
    assert(w == FALSE);
    flush_calls++;
    if (keep == FALSE) {
        flush_evict_calls++;
	if (verbose) printf("%s:%d flush %"PRId64"\n", __FUNCTION__, __LINE__, k.b);
        evicted_keys |= 1 << k.b;
    }
}

static int fetch_calls = 0;

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       CACHEKEY k,
       u_int32_t fullhash __attribute__((__unused__)),
       void **value,
       long *sizep,
       void *extraargs    __attribute__((__unused__))
       ) {

    fetch_calls++;
    if (verbose) printf("%s:%d %"PRId64"\n", __FUNCTION__, __LINE__, k.b);

    *value = 0;
    *sizep = 1;

    return 0;
}

// Note: cachetable_size_limit must be a power of 2
static void cachetable_prefetch_flowcontrol_test (int cachetable_size_limit) {
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, cachetable_size_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    int i;

    // prefetch keys 0 .. N-1.  they should all fit in the cachetable
    for (i=0; i<cachetable_size_limit; i++) {
        CACHEKEY key = make_blocknum(i);
        u_int32_t fullhash = toku_cachetable_hash(f1, key);
        r = toku_cachefile_prefetch(f1, key, fullhash, flush, fetch, 0);
        toku_cachetable_verify(ct);
    }

    // wait for all of the blocks to be fetched
    sleep(10);

    // prefetch keys N .. 2*N-1.  0 .. N-1 should be evicted.
    for (i=i; i<2*cachetable_size_limit; i++) {
        CACHEKEY key = make_blocknum(i);
        u_int32_t fullhash = toku_cachetable_hash(f1, key);
        r = toku_cachefile_prefetch(f1, key, fullhash, flush, fetch, 0);
        toku_cachetable_verify(ct);
	// sleep(1);
    }

    // wait for everything to finish
    sleep(10);
#if 0 //If we flush using reader thread.
    assert(flush_evict_calls == cachetable_size_limit);
    assert(evicted_keys == (1 << cachetable_size_limit)-1);
#else
    assert(flush_evict_calls == 0);
    assert(evicted_keys == 0);
#endif

    char *error_string;
    r = toku_cachefile_close(&f1, &error_string, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    if (verbose) printf("%s:%d 0x%x 0x%x\n", __FUNCTION__, __LINE__,
	evicted_keys, (1 << (2*cachetable_size_limit))-1);
    assert(evicted_keys == (1 << (2*cachetable_size_limit))-1);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_prefetch_flowcontrol_test(8);
    return 0;
}
