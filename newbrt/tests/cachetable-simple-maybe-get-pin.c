#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

//
// simple tests for maybe_get_and_pin(_clean)
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

static void
cachetable_test (void) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    long s1;
    // nothing in cachetable, so this should fail
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, flush, fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, NULL, NULL);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

    // maybe_get_and_pin_clean should succeed, maybe_get_and_pin should fail
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    r = toku_cachetable_maybe_get_and_pin_clean(f1, make_blocknum(1), 1, &v1);
    assert(r == 0);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));
    // maybe_get_and_pin_clean should succeed, maybe_get_and_pin should fail
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==0);
    // now these calls should fail because the node is already pinned, and therefore in use
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    r = toku_cachetable_maybe_get_and_pin_clean(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));

    // sanity check, this should still succeed, because the PAIR is dirty
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==0);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));
    r = toku_cachetable_begin_checkpoint(ct, NULL); assert(r == 0);
    // now these should fail, because the node should be pending a checkpoint
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
    r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(1), 1, &v1);
    assert(r==-1);
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
  cachetable_test();
  return 0;
}
