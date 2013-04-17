/* -*- mode: C; c-basic-offset: 4 -*- */

// verify that cachetable prefetch of the same block multiple times only
// fetches the block once.
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void *e     __attribute__((__unused__)),
       long s      __attribute__((__unused__)),
       BOOL w      __attribute__((__unused__)),
       BOOL keep   __attribute__((__unused__)),
       BOOL c      __attribute__((__unused__))
       ) {
}

static int fetch_calls = 0;

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

    fetch_calls++;
    sleep(10);

    *value = 0;
    *sizep = 1;
    *dirtyp = 1;

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
    *bytes_freed = 0;
    return 0;
}

static BOOL pf_req_callback(void* UU(brtnode_pv), void* UU(read_extraargs)) {
    return FALSE;
}

static int pf_callback(void* UU(brtnode_pv), void* UU(read_extraargs), int UU(fd), long* UU(sizep)) {
    assert(FALSE);
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
    r = toku_cachefile_prefetch(f1, key, fullhash, flush, fetch, pe_est_callback, pe_callback, pf_req_callback, pf_callback, 0, 0, NULL);
    toku_cachetable_verify(ct);

    // prefetch again. this should do nothing.
    r = toku_cachefile_prefetch(f1, key, fullhash, flush, fetch, pe_est_callback, pe_callback, pf_req_callback, pf_callback, 0, 0, NULL);
    toku_cachetable_verify(ct);

    // verify that maybe_get_and_pin returns an error while the prefetch is in progress
    int i;
    for (i=1; i>=0; i++) {
        void *v;
        r = toku_cachetable_maybe_get_and_pin(f1, key, fullhash, &v);
        if (r == 0) break;
        toku_pthread_yield();
    }
    if (verbose) printf("%s:%d %d\n", __FUNCTION__, __LINE__, i);
    assert(i>1);
    toku_cachetable_verify(ct);

    // there should only be 1 fetch callback
    assert(fetch_calls == 1);

    r = toku_cachetable_unpin(f1, key, fullhash, CACHETABLE_CLEAN, 1);
    assert(r == 0);
    toku_cachetable_verify(ct);

    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_prefetch_maybegetandpin_test();
    return 0;
}
