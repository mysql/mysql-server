/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

// verify that closing the cachetable with an in progress prefetch works
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include "test.h"

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void** UU(dd),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       bool w      __attribute__((__unused__)),
       bool keep   __attribute__((__unused__)),
       bool c      __attribute__((__unused__)),
        bool UU(is_clone)
       ) {
    assert(w == false && v != NULL);
    toku_free(v);
}

static int fetch_calls = 0;

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       void** UU(dd),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp       __attribute__((__unused__)),
       void *extraargs    __attribute__((__unused__))
       ) {

    fetch_calls++;
    sleep(10);

    *value = toku_malloc(1);
    *sizep = make_pair_attr(1);
    *dirtyp = 0;

    return 0;
}

static void cachetable_prefetch_close_leak_test (void) {
    const int test_limit = 1;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    // prefetch block 0. this will take 10 seconds.
    CACHEKEY key = make_blocknum(0);
    uint32_t fullhash = toku_cachetable_hash(f1, make_blocknum(0));
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;
    r = toku_cachefile_prefetch(f1, key, fullhash, wc, fetch, def_pf_req_callback, def_pf_callback, 0, NULL);
    toku_cachetable_verify(ct);

    // close with the prefetch in progress. the close should block until
    // all of the reads and writes are complete.
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_prefetch_close_leak_test();
    return 0;
}
