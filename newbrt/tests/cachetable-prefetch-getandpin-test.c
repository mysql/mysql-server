/* -*- mode: C; c-basic-offset: 4 -*- */

/* verify that get_and_pin waits while a prefetch block is pending */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

BOOL do_pf;
BOOL expect_pf;

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
    assert(w == FALSE);
}

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       u_int32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       long *sizep        __attribute__((__unused__)),
       int  *dirtyp       __attribute__((__unused__)),
       void *extraargs    __attribute__((__unused__))
       ) {

    if(!expect_pf) {
        sleep(2);
    }
    *value = 0;
    *sizep = 2;
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

static BOOL pf_req_callback(void* UU(brtnode_pv), void* UU(read_extraargs)) {
    if (do_pf) {
        assert(expect_pf);
        return TRUE;
    }
    else {
        return FALSE;
    }
}

static int pf_callback(void* UU(brtnode_pv), void* UU(read_extraargs), int UU(fd), long* UU(sizep)) {
    assert(expect_pf);
    sleep(2);
    *sizep = 2;
    return 0;
}

static uint64_t tdelta_usec(struct timeval *tend, struct timeval *tstart) {
    uint64_t t = tend->tv_sec * 1000000 + tend->tv_usec;
    t -= tstart->tv_sec * 1000000 + tstart->tv_usec;
    return t;
}

static void cachetable_prefetch_maybegetandpin_test (BOOL do_partial_fetch) {
    const int test_limit = 2;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    expect_pf = FALSE;
    do_pf = FALSE;
    CACHEKEY key = make_blocknum(0);
    u_int32_t fullhash = toku_cachetable_hash(f1, make_blocknum(0));
    if (do_partial_fetch) {
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
            pe_est_callback, 
            pe_callback, 
            pf_req_callback,
            pf_callback,
            0,
            0
            );
        assert(r==0);
        r = toku_cachetable_unpin(f1, key, fullhash, CACHETABLE_CLEAN, 1);
    }

    struct timeval tstart;
    gettimeofday(&tstart, NULL);

    // prefetch block 0. this will take 2 seconds.
    do_pf = TRUE;
    r = toku_cachefile_prefetch(f1, key, fullhash, flush, fetch, pe_est_callback, pe_callback, pf_req_callback, pf_callback, 0, 0, NULL);
    toku_cachetable_verify(ct);

    // verify that get_and_pin waits while the prefetch is in progress
    void *v = 0;
    long size = 0;
    do_pf = FALSE;
    r = toku_cachetable_get_and_pin_nonblocking(f1, key, fullhash, &v, &size, flush, fetch, pe_est_callback, pe_callback, pf_req_callback, pf_callback, NULL, NULL, NULL);
    assert(r==TOKUDB_TRY_AGAIN);
    r = toku_cachetable_get_and_pin(f1, key, fullhash, &v, &size, flush, fetch, pe_est_callback, pe_callback, pf_req_callback, pf_callback, NULL, NULL);
    assert(r == 0 && v == 0 && size == 2);

    struct timeval tend; 
    gettimeofday(&tend, NULL);

    assert(tdelta_usec(&tend, &tstart) >= 2000000); 
    
    toku_cachetable_verify(ct);

    r = toku_cachetable_unpin(f1, key, fullhash, CACHETABLE_CLEAN, 1);
    assert(r == 0);
    toku_cachetable_verify(ct);

    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_prefetch_maybegetandpin_test(TRUE);
    cachetable_prefetch_maybegetandpin_test(FALSE);
    return 0;
}
