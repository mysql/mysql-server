#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

BOOL foo;
BOOL check_me;
BOOL flush_called;

//
// This test verifies that get_and_pin takes a write lock on a PAIR.
//

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
  /* Do nothing */
  if (verbose) { printf("FLUSH: %d\n", (int)k.b); }
  //usleep (5*1024*1024);
  if (check_me) {
    flush_called = TRUE;
    assert(c);
    assert(keep);
    assert(w);
  }
}

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       u_int32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
  *dirtyp = 0;
  *value = NULL;
  *sizep = make_pair_attr(8);
  return 0;
}

static void kibbutz_work(void *fe_v)
{
    CACHEFILE f1 = fe_v;
    sleep(2);
    foo = TRUE;
    int r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r==0);
    remove_background_job(f1, false);    
}

static void
run_test (void) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    //void* v2;
    long s1;
    //long s2;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, flush, fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, NULL, NULL);
    foo = FALSE;
    cachefile_kibbutz_enq(f1, kibbutz_work, f1);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, flush, fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, NULL, NULL);
    assert(foo);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

    //now let's do a simple checkpoint test
    // first dirty the PAIR
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, flush, fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, NULL, NULL);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));

    // now this should mark the pair for checkpoint
    r = toku_cachetable_begin_checkpoint(ct, NULL);

    //
    // now we pin the pair again, and verify in flush callback that the pair is being checkpointed
    //
    check_me = TRUE;
    flush_called = FALSE;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, flush, fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, NULL, NULL);
    assert(flush_called);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

    
    check_me = FALSE;
    r = toku_cachetable_end_checkpoint(
        ct, 
        NULL, 
        fake_ydb_lock,
        fake_ydb_unlock,
        NULL,
        NULL
        );
    assert(r==0);
    
    
    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);
    
    
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  run_test();
  return 0;
}
