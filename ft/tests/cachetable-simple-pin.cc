/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"
#include "cachetable-test.h"

bool foo;
bool check_me;
bool flush_called;

//
// This test verifies that get_and_pin takes a write lock on a PAIR.
//

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
  /* Do nothing */
  if (verbose) { printf("FLUSH: %d\n", (int)k.b); }
  //usleep (5*1024*1024);
  if (check_me) {
    flush_called = true;
    assert(c);
    assert(keep);
    assert(w);
  }
}

static void kibbutz_work(void *fe_v)
{
    CACHEFILE CAST_FROM_VOIDP(f1, fe_v);
    sleep(2);
    foo = true;
    int r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r==0);
    remove_background_job_from_cf(f1);    
}

static void
run_test (void) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    create_dummy_functions(f1);

    void* v1;
    //void* v2;
    long s1;
    //long s2;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    foo = false;
    cachefile_kibbutz_enq(f1, kibbutz_work, f1);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    assert(foo);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

    //now let's do a simple checkpoint test
    // first dirty the PAIR
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));

    // now this should mark the pair for checkpoint
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    toku_cachetable_begin_checkpoint(cp, NULL);

    //
    // now we pin the pair again, and verify in flush callback that the pair is being checkpointed
    //
    check_me = true;
    flush_called = false;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    assert(flush_called);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

    
    check_me = false;
    toku_cachetable_end_checkpoint(
        cp, 
        NULL, 
        NULL,
        NULL
        );
    
    toku_cachetable_verify(ct);
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  run_test();
  return 0;
}
