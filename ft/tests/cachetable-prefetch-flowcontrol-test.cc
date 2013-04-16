/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

// verify that cachetable prefetch multiple blocks hits the cachetable size limit
// and flushes eventually happen.
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."


#include "test.h"
#include "cachetable-internal.h"

static int flush_calls = 0;
static int flush_evict_calls = 0;
static int evicted_keys = 0;

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k,
       void *v     __attribute__((__unused__)),
       void** UU(dd),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       bool w,
       bool keep,
       bool f_ckpt __attribute__((__unused__)),
        bool UU(is_clone)
       ) {
    assert(w == false);
    sleep(1);
    flush_calls++;
    if (keep == false) {
        flush_evict_calls++;
        if (verbose) printf("%s:%d flush %" PRId64 "\n", __FUNCTION__, __LINE__, k.b);
        evicted_keys |= 1 << k.b;
    }
}

static int fetch_calls = 0;

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k,
       uint32_t fullhash __attribute__((__unused__)),
       void **value,
       void** UU(dd),
       PAIR_ATTR *sizep,
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {

    fetch_calls++;
    if (verbose) printf("%s:%d %" PRId64 "\n", __FUNCTION__, __LINE__, k.b);

    *value = 0;
    *sizep = make_pair_attr(1);
    *dirtyp = 0;

    return 0;
}

// Note: cachetable_size_limit must be a power of 2
static void cachetable_prefetch_flowcontrol_test (int cachetable_size_limit) {
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, cachetable_size_limit, ZERO_LSN, NULL_LOGGER);
    evictor_test_helpers::set_hysteresis_limits(&ct->ev, cachetable_size_limit, cachetable_size_limit);
    evictor_test_helpers::disable_ev_thread(&ct->ev);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    int i;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;

    // prefetch keys 0 .. N-1.  they should all fit in the cachetable
    for (i=0; i<cachetable_size_limit+1; i++) {
        CACHEKEY key = make_blocknum(i);
        uint32_t fullhash = toku_cachetable_hash(f1, key);
        bool doing_prefetch = false;
        r = toku_cachefile_prefetch(f1, key, fullhash, wc, fetch, def_pf_req_callback, def_pf_callback, 0, &doing_prefetch);
        assert(doing_prefetch);
        toku_cachetable_verify(ct);
    }

    // wait for all of the blocks to be fetched
    sleep(3);

    // prefetch keys N .. 2*N-1.  0 .. N-1 should be evicted.
    for (i=i+1; i<2*cachetable_size_limit; i++) {
        CACHEKEY key = make_blocknum(i);
        uint32_t fullhash = toku_cachetable_hash(f1, key);
        bool doing_prefetch = false;
        r = toku_cachefile_prefetch(f1, key, fullhash, wc, fetch, def_pf_req_callback, def_pf_callback, 0, &doing_prefetch);
        assert(!doing_prefetch);
        toku_cachetable_verify(ct);
    }


    toku_cachefile_close(&f1, false, ZERO_LSN);
    if (verbose) printf("%s:%d 0x%x 0x%x\n", __FUNCTION__, __LINE__,
        evicted_keys, (1 << (2*cachetable_size_limit))-1);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_prefetch_flowcontrol_test(8);
    return 0;
}
