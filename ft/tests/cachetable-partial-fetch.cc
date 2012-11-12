/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include "test.h"

//
// This file contains some basic tests for partial fetch, ensuring that 
// it works correctly
//

uint32_t fetch_val = 0;
bool pf_req_called;

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       void** UU(dd),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
  *dirtyp = 0;
  *value = &fetch_val;
  *sizep = make_pair_attr(sizeof(fetch_val));
  return 0;
}

static int
err_fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
	   void** UU(dd),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
  assert(false);
  *dirtyp = 0;
  return 0;
}

static bool pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
  return false;
}

static bool true_pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
    if (pf_req_called) return false;
    return true;
}

static int err_pf_callback(void* UU(ftnode_pv), void* UU(dd), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* UU(sizep)) {
  assert(false);
  return 0; // gcov
}

static int pf_callback(void* UU(ftnode_pv), void* UU(dd), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* UU(sizep)) {
  assert(false);
  return 0; // gcov
}

static int true_pf_callback(void* UU(ftnode_pv), void* UU(dd), void* read_extraargs, int UU(fd), PAIR_ATTR* sizep) {
    pf_req_called = true;
    *sizep = make_pair_attr(sizeof(fetch_val)+1);
    assert(read_extraargs == &fetch_val);
    return 0;
}


static void
cachetable_test (void) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    bool doing_prefetch = false;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    //void* v2;
    long s1;
    //long s2;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, fetch, pf_req_callback, pf_callback, true, NULL);
    assert(&fetch_val == v1);
    //
    // verify that a prefetch of this node will fail
    //
    r = toku_cachefile_prefetch(
        f1,
        make_blocknum(1),
        1,
        wc,
        fetch,
        pf_req_callback,
        pf_callback,
        NULL,
        &doing_prefetch
        );
    assert(r == 0);
    // make sure that prefetch should not happen, because we have already pinned node
    assert(!doing_prefetch);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    //
    // now get and pin node again, and make sure that partial fetch and fetch are not called
    //
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, err_fetch, pf_req_callback, err_pf_callback, true, NULL);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    //
    // now make sure that if we say a partial fetch is required, that we get a partial fetch
    // and that read_extraargs properly passed down
    //
    pf_req_called = false;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, err_fetch, true_pf_req_callback, true_pf_callback, true, &fetch_val);
    assert(pf_req_called);
    assert(s1 == sizeof(fetch_val)+1);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

    // close and reopen cachefile so we can do some simple prefetch tests
    toku_cachefile_close(&f1, false, ZERO_LSN);
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    //
    // verify that a prefetch of the node will succeed
    //
    r = toku_cachefile_prefetch(
        f1,
        make_blocknum(1),
        1,
        wc,
        fetch,
        pf_req_callback,
        pf_callback,
        NULL,
        &doing_prefetch
        );
    assert(r == 0);
    // make sure that prefetch should happen, because we have already pinned node
    assert(doing_prefetch);
    //
    // now verify we can pin it, and NO fetch callback should get called
    //
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, err_fetch, pf_req_callback, err_pf_callback, true, NULL);
    assert(&fetch_val == v1);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

    //
    // now verify a prefetch that requires a partial fetch works, and that we can then pin the node
    //
    pf_req_called = false;
    r = toku_cachefile_prefetch(
        f1,
        make_blocknum(1),
        1,
        wc,
        fetch,
        true_pf_req_callback,
        true_pf_callback,
        &fetch_val,
        &doing_prefetch
        );
    assert(doing_prefetch);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, err_fetch, pf_req_callback, err_pf_callback, true, NULL);
    assert(&fetch_val == v1);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    
    
    toku_cachetable_verify(ct);
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
    
    
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_test();
    return 0;
}
