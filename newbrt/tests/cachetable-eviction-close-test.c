/* -*- mode: C; c-basic-offset: 4 -*- */

// verify that closing the cachetable with prefetches in progress works
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

BOOL check_flush;
BOOL expect_full_flush;
BOOL expect_pe;

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
    assert(expect_full_flush);
    sleep(2);
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

    *value = 0;
    *sizep = make_pair_attr(8);
    *dirtyp = 0;

    return 0;
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
    *cost = PE_EXPENSIVE;
}

static void cachetable_eviction_full_test (void) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    CACHEKEY key = make_blocknum(0);
    u_int32_t fullhash = toku_cachetable_hash(f1, make_blocknum(0));

    void* value1;
    long size1;
    void* value2;
    long size2;
    //
    // let's pin a node multiple times
    // and really bring up its clock count
    //
    for (int i = 0; i < 20; i++) {
        r = toku_cachetable_get_and_pin(
            f1, 
            key, 
            fullhash, 
            &value1, 
            &size1, 
            flush, 
            fetch,
            pe_est_callback, 
            def_pe_callback, 
            def_pf_req_callback,
            def_pf_callback,
            def_cleaner_callback,
            0,
            0
            );
        assert(r==0);
        r = toku_cachetable_unpin(f1, key, fullhash, CACHETABLE_DIRTY, make_pair_attr(1));
        assert(r == 0);
    }
    expect_full_flush = TRUE;
    // now pin a different, causing an eviction
    r = toku_cachetable_get_and_pin(
        f1, 
        make_blocknum(1), 
        1, 
        &value2, 
        &size2, 
        def_flush, 
        fetch,
        pe_est_callback, 
        def_pe_callback, 
        def_pf_req_callback,
        def_pf_callback,
        def_cleaner_callback,
        0,
        0
        );
    assert(r==0);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(1));
    assert(r == 0);
    toku_cachetable_verify(ct);

    // close with the eviction in progress. the close should block until
    // all of the reads and writes are complete.
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_eviction_full_test();
    return 0;
}
