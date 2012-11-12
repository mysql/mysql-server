/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/* verify that get_and_pin waits while a prefetch block is pending */
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include "test.h"

bool do_pf;
bool expect_pf;

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
    assert(w == false);
}

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

    if(!expect_pf) {
        sleep(2);
    }
    *value = 0;
    *sizep = make_pair_attr(2);
    *dirtyp = 0;

    return 0;
}

static bool pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
    if (do_pf) {
        assert(expect_pf);
        return true;
    }
    else {
        return false;
    }
}

static int pf_callback(void* UU(ftnode_pv), void* UU(dd), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* UU(sizep)) {
    assert(expect_pf);
    sleep(2);
    *sizep = make_pair_attr(2);
    return 0;
}

static uint64_t tdelta_usec(struct timeval *tend, struct timeval *tstart) {
    uint64_t t = tend->tv_sec * 1000000 + tend->tv_usec;
    t -= tstart->tv_sec * 1000000 + tstart->tv_usec;
    return t;
}

static void cachetable_prefetch_maybegetandpin_test (bool do_partial_fetch) {
    const int test_limit = 2;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    expect_pf = false;
    do_pf = false;
    CACHEKEY key = make_blocknum(0);
    uint32_t fullhash = toku_cachetable_hash(f1, make_blocknum(0));
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;
    if (do_partial_fetch) {
        expect_pf = true;
        void* value;
        long size;
        r = toku_cachetable_get_and_pin(
            f1, 
            key, 
            fullhash, 
            &value, 
            &size, 
            wc, 
            fetch,
            pf_req_callback,
            pf_callback,
            true, 
            0
            );
        assert(r==0);
        r = toku_test_cachetable_unpin(f1, key, fullhash, CACHETABLE_CLEAN, make_pair_attr(1));
    }

    struct timeval tstart;
    gettimeofday(&tstart, NULL);

    // prefetch block 0. this will take 2 seconds.
    do_pf = true;
    r = toku_cachefile_prefetch(f1, key, fullhash, wc, fetch, pf_req_callback, pf_callback, 0, NULL);
    toku_cachetable_verify(ct);

    // verify that get_and_pin waits while the prefetch is in progress
    void *v = 0;
    long size = 0;
    do_pf = false;
    r = toku_cachetable_get_and_pin_nonblocking(f1, key, fullhash, &v, &size, wc, fetch, pf_req_callback, pf_callback, PL_WRITE_EXPENSIVE, NULL, NULL);
    assert(r==TOKUDB_TRY_AGAIN);
    r = toku_cachetable_get_and_pin(f1, key, fullhash, &v, &size, wc, fetch, pf_req_callback, pf_callback, true, NULL);
    assert(r == 0 && v == 0 && size == 2);

    struct timeval tend;
    gettimeofday(&tend, NULL);

    assert(tdelta_usec(&tend, &tstart) >= 1900000);

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
    cachetable_prefetch_maybegetandpin_test(true);
    cachetable_prefetch_maybegetandpin_test(false);
    return 0;
}
