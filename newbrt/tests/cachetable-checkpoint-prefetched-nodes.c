#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

u_int64_t clean_val = 0;
u_int64_t dirty_val = 0;

BOOL check_me;
BOOL flush_called;

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
  // if the checkpoint is pending, assert that it is of what we made dirty
  if (check_me) {
    flush_called = TRUE;
    assert(c);
    assert(e == &dirty_val);
    assert(v == &dirty_val);
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
  if (extraargs) {
      *value = &dirty_val;
      *dirtyp = TRUE;
  }
  else {
      *value = &clean_val;
      *dirtyp = FALSE;
  }
  *sizep = make_pair_attr(8);
  return 0;
}

static void
cachetable_test (void) {
  const int test_limit = 20;
  int r;
  CACHETABLE ct;
  r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
  char fname1[] = __FILE__ "test1.dat";
  unlink(fname1);
  CACHEFILE f1;
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

  BOOL doing_prefetch = FALSE;
  r = toku_cachefile_prefetch(f1, make_blocknum(1), 1, flush, fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, &dirty_val, &dirty_val, &doing_prefetch);
  assert(doing_prefetch);
  doing_prefetch = FALSE;
  r = toku_cachefile_prefetch(f1, make_blocknum(2), 2, flush, fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, NULL, NULL, &doing_prefetch);
  assert(doing_prefetch);

  //
  // Here is the test, we have two pairs, v1 is dirty, v2 is clean, but both are currently pinned
  // Then we will begin a checkpoint, which should theoretically mark both as pending, but
  // flush will be called only for v1, because v1 is dirty
  //

  r = toku_cachetable_begin_checkpoint(ct, NULL); assert(r == 0);


  check_me = TRUE;
  flush_called = FALSE;
  r = toku_cachetable_end_checkpoint(
      ct, 
      NULL, 
      fake_ydb_lock,
      fake_ydb_unlock,
      NULL,
      NULL
      );
  assert(r==0);
  assert(flush_called);
  check_me = FALSE;
  


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
