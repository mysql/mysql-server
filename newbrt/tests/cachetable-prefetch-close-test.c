/* -*- mode: C; c-basic-offset: 4 -*- */

// verify that closing the cachetable with prefetches in progress works
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

BOOL expect_pf;

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       BOOL w      __attribute__((__unused__)),
       BOOL keep   __attribute__((__unused__)),
       BOOL c      __attribute__((__unused__))
       ) {
    assert(w == FALSE);
}

static int fetch_calls = 0;

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       u_int32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp       __attribute__((__unused__)),
       void *extraargs    __attribute__((__unused__))
       ) {

    fetch_calls++;
    sleep(2);

    *value = 0;
    *sizep = make_pair_attr(1);
    *dirtyp = 0;

    return 0;
}

static void cachetable_prefetch_full_test (BOOL partial_fetch) {
    const int test_limit = 2;
    expect_pf = FALSE;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    // prefetch block 0. this will take 2 seconds.
    CACHEKEY key = make_blocknum(0);
    u_int32_t fullhash = toku_cachetable_hash(f1, make_blocknum(0));

    // if we want to do a test of partial fetch,
    // we first put the key into the cachefile so that
    // the subsequent prefetch does a partial fetch
    if (partial_fetch) {
        expect_pf = TRUE;
        void* value;
        long size;
        r = toku_cachetable_get_and_pin(
            f1, 
            key, 
            fullhash, 
            &value, 
            &size, 
            flush, 
            fetch,
            def_pe_est_callback, 
            def_pe_callback, 
            def_pf_req_callback,
            def_pf_callback,
            def_cleaner_callback,
            0,
            0
            );
        assert(r==0);
        r = toku_cachetable_unpin(f1, key, fullhash, CACHETABLE_CLEAN, make_pair_attr(1));
    }
    
    r = toku_cachefile_prefetch(f1, key, fullhash, flush, fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, 0, 0, NULL);
    toku_cachetable_verify(ct);

    // close with the prefetch in progress. the close should block until
    // all of the reads and writes are complete.
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_prefetch_full_test(TRUE);
    cachetable_prefetch_full_test(FALSE);
    return 0;
}
