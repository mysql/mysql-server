/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "test.h"

CACHEFILE f1;
CACHEFILE f2;

bool check_flush;
bool dirty_flush_called;
bool check_pe_callback;
bool pe_callback_called;

CACHETABLE ct;

static int 
pe_callback (
    void *ftnode_pv __attribute__((__unused__)), 
    PAIR_ATTR bytes_to_free __attribute__((__unused__)), 
    PAIR_ATTR* bytes_freed, 
    void* extraargs __attribute__((__unused__))
    ) 
{
    *bytes_freed = make_pair_attr(1);
    if (check_pe_callback) {
        pe_callback_called = true;
    }
    usleep(4*1024*1024);
    return 0;
}

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void **dd     __attribute__((__unused__)),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       bool w      __attribute__((__unused__)),
       bool keep   __attribute__((__unused__)),
       bool c      __attribute__((__unused__)),
       bool UU(is_clone)
       ) {
    if (check_flush && w) {
        dirty_flush_called = true;
    }
}

static void *f2_pin(void *arg) {
    int r;    
    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    //
    // these booleans for pe_callback just ensure that the
    // test is working as we expect it to. We expect the get_and_pin to 
    // cause a partial eviction of f1's PAIR, reducing its size from 8 to 1
    // and we expect that to be enough so that the unpin does not invoke a partial eviction
    // This is just to ensure that the bug is being exercised
    //
    check_pe_callback = true;
    r = toku_cachetable_get_and_pin(f2, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    assert(r == 0);
    ct->ev.signal_eviction_thread();
    usleep(1*1024*1024);
    assert(pe_callback_called);
    pe_callback_called = false;
    r = toku_test_cachetable_unpin(f2, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    check_pe_callback = false;
    assert(!pe_callback_called);
    assert(r == 0);
    
    return arg;
}

static void
cachetable_test (void) {
    const int test_limit = 12;
    int r;
    check_flush = false;
    dirty_flush_called = false;
    
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    evictor_test_helpers::disable_ev_thread(&ct->ev); // disable eviction thread
    
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    char fname2[] = __SRCFILE__ "test2.dat";
    unlink(fname2);

    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); 
    assert(r == 0);
    r = toku_cachetable_openf(&f2, ct, fname2, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); 
    assert(r == 0);

    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.pe_callback = pe_callback;
    wc.flush_callback = flush;
    // pin and unpin a node 20 times, just to get clock count up
    for (int i = 0; i < 20; i++) {
        r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
        assert(r == 0);
        r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));
        assert(r == 0);
    }

    // at this point, we have a dirty PAIR in the cachetable associated with cachefile f1
    // launch a thread that will put another PAIR in the cachetable, and get partial eviction started
    toku_pthread_t tid;
    r = toku_pthread_create(&tid, NULL, f2_pin, NULL); 
    assert_zero(r);

    usleep(2*1024*1024);
    check_flush = true;
    toku_cachefile_close(&f1, false, ZERO_LSN); 
    assert(dirty_flush_called);
    check_flush = false;

    void *ret;
    r = toku_pthread_join(tid, &ret); 
    assert_zero(r);


    toku_cachetable_verify(ct);
    toku_cachefile_close(&f2, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test();
  return 0;
}
