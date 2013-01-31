/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/* verify that get_and_pin waits while a prefetch block is pending */
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include "test.h"


static void 
pe_est_callback(
    void* UU(ftnode_pv), 
    void* UU(dd),
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    *bytes_freed_estimate = 7;
    *cost = PE_EXPENSIVE;
}

static int 
pe_callback (
    void *ftnode_pv __attribute__((__unused__)), 
    PAIR_ATTR bytes_to_free __attribute__((__unused__)), 
    PAIR_ATTR* bytes_freed, 
    void* extraargs __attribute__((__unused__))
    ) 
{
    sleep(3);
    *bytes_freed = make_pair_attr(bytes_to_free.size-7);
    return 0;
}

static uint64_t tdelta_usec(struct timeval *tend, struct timeval *tstart) {
    uint64_t t = tend->tv_sec * 1000000 + tend->tv_usec;
    t -= tstart->tv_sec * 1000000 + tstart->tv_usec;
    return t;
}

static void cachetable_prefetch_maybegetandpin_test (void) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    evictor_test_helpers::disable_ev_thread(&ct->ev);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    CACHEKEY key = make_blocknum(0);
    uint32_t fullhash = toku_cachetable_hash(f1, make_blocknum(0));

    // let's get and pin this node a bunch of times to drive up the clock count
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.pe_est_callback = pe_est_callback;
    wc.pe_callback = pe_callback;
    for (int i = 0; i < 20; i++) {
        void* value;
        long size;
        r = toku_cachetable_get_and_pin(
            f1, 
            key, 
            fullhash, 
            &value, 
            &size, 
            wc, 
            def_fetch,
            def_pf_req_callback,
            def_pf_callback,
            true, 
            0
            );
        assert(r==0);
        r = toku_test_cachetable_unpin(f1, key, fullhash, CACHETABLE_DIRTY, make_pair_attr(8));
    }
    
    struct timeval tstart;
    gettimeofday(&tstart, NULL);

    // fetch another block, causing an eviction of the first block we made above
    void* value2;
    long size2;
    r = toku_cachetable_get_and_pin(
        f1,
        make_blocknum(1),
        1,
        &value2,
        &size2,
        wc, 
        def_fetch,
        def_pf_req_callback,
        def_pf_callback,
        true, 
        0
        );
    assert(r==0);
    ct->ev.signal_eviction_thread();
    usleep(1*1024*1024);        
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

        
    toku_cachetable_verify(ct);

    void *v = 0;
    long size = 0;
    // now verify that the block we are trying to evict may be pinned
    r = toku_cachetable_get_and_pin_nonblocking(
        f1, 
        key, 
        fullhash, 
        &v, 
        &size, 
        wc, 
        def_fetch, 
        def_pf_req_callback, 
        def_pf_callback, 
        PL_WRITE_EXPENSIVE,
        NULL, 
        NULL
        );
    assert(r==TOKUDB_TRY_AGAIN);
    r = toku_cachetable_get_and_pin(
        f1, 
        key, 
        fullhash, 
        &v, 
        &size, 
        wc, 
        def_fetch, 
        def_pf_req_callback, 
        def_pf_callback, 
        true, 
        NULL
        );
    assert(r == 0 && v == 0 && size == 1);

    struct timeval tend; 
    gettimeofday(&tend, NULL);

    assert(tdelta_usec(&tend, &tstart) >= 2000000); 
    if (verbose) printf("time %" PRIu64 " \n", tdelta_usec(&tend, &tstart));
    toku_cachetable_verify(ct);

    r = toku_test_cachetable_unpin(f1, key, fullhash, CACHETABLE_CLEAN, make_pair_attr(1));
    assert(r == 0);
    toku_cachetable_verify(ct);

    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_prefetch_maybegetandpin_test();
    return 0;
}
