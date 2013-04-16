/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include "test.h"

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
    *dirtyp = 0;
    *sizep = make_pair_attr(0);
    return 0;
}

// test simple unpin and remove
static void
cachetable_unpin_and_remove_test (int n) {
    if (verbose) printf("%s %d\n", __FUNCTION__, n);
    const int table_limit = 2*n;
    int r;
    int i;

    CACHETABLE ct;
    toku_cachetable_create(&ct, table_limit, ZERO_LSN, NULL_LOGGER);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, 0777); assert(r == 0);
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);

    // generate some random keys
    CACHEKEY keys[n]; int nkeys = n;
    for (i=0; i<n; i++) {
        keys[i].b = random();
    }

    // put the keys into the cachetable
    for (i=0; i<n; i++) {
        uint32_t hi = toku_cachetable_hash(f1, make_blocknum(keys[i].b));
        toku_cachetable_put(f1, make_blocknum(keys[i].b), hi, (void *)(long) keys[i].b, make_pair_attr(1),wc, put_callback_nop);
    }
    
    // unpin and remove
    CACHEKEY testkeys[n];
    for (i=0; i<n; i++) testkeys[i] = keys[i];
    while (nkeys > 0) {
        i = random() % nkeys;
        uint32_t hi = toku_cachetable_hash(f1, make_blocknum(testkeys[i].b));
        r = toku_test_cachetable_unpin_and_remove(f1, testkeys[i], NULL, NULL);
        assert(r == 0);

        toku_cachefile_verify(f1);

        // verify that k is removed
        void *v;
        r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(testkeys[i].b), hi, PL_WRITE_EXPENSIVE, &v);
        assert(r != 0);

        testkeys[i] = testkeys[nkeys-1]; nkeys -= 1;
    }

    // verify that the cachtable is empty
    int nentries;
    toku_cachetable_get_state(ct, &nentries, NULL, NULL, NULL);
    assert(nentries == 0);

    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

// test remove when the pair in being written
static void
cachetable_put_evict_remove_test (int n) {
    if (verbose) printf("%s %d\n", __FUNCTION__, n);
    const int table_limit = n-1;
    int r;
    int i;

    CACHETABLE ct;
    toku_cachetable_create(&ct, table_limit, ZERO_LSN, NULL_LOGGER);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, 0777); assert(r == 0);
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);

    uint32_t hi[n];
    for (i=0; i<n; i++)
        hi[i] = toku_cachetable_hash(f1, make_blocknum(i));

    // put 0, 1, 2, ... should evict 0
    for (i=0; i<n; i++) {
        toku_cachetable_put(f1, make_blocknum(i), hi[i], (void *)(long)i, make_pair_attr(1), wc, put_callback_nop);
        r = toku_test_cachetable_unpin(f1, make_blocknum(i), hi[i], CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }

    // get 0
    void *v; long s;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(0), hi[0], &v, &s, wc, fetch, def_pf_req_callback, def_pf_callback, true, 0);
    assert(r == 0);
        
    // remove 0
    r = toku_test_cachetable_unpin_and_remove(f1, make_blocknum(0), NULL, NULL);
    assert(r == 0);

    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_unpin_and_remove_test(8);
    cachetable_put_evict_remove_test(4);
    return 0;
}
