/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

CACHEFILE f1;

BOOL flush_called;

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void** UU(dd),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       BOOL w      __attribute__((__unused__)),
       BOOL keep   __attribute__((__unused__)),
       BOOL c      __attribute__((__unused__)),
        BOOL UU(is_clone)
       ) {
  /* Do nothing */
  if (verbose) { printf("FLUSH: %d\n", (int)k.b); }
  //usleep (5*1024*1024);
  PAIR_ATTR attr = make_pair_attr(8);
  attr.cache_pressure_size = 0;
  *new_size = attr;
  if (w) {
      assert(!flush_called);
      assert(c);
      flush_called = TRUE;
  }
}

BOOL cleaner_called;

static int
cleaner_callback(
    void* UU(ftnode_pv),
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    void* UU(extraargs)
    )
{
    assert(blocknum.b == 1);
    assert(fullhash == 1);
    assert(!cleaner_called);
    assert(flush_called);
    cleaner_called = TRUE;
    int r = toku_cachetable_unpin(f1, blocknum, fullhash, CACHETABLE_CLEAN, make_pair_attr(8));
    assert_zero(r);
    return 0;
}


static void
cachetable_test (void) {
  const int test_limit = 12;
  int r;
  CACHETABLE ct;
  r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
  char fname1[] = __SRCFILE__ "test1.dat";
  unlink(fname1);
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

  void* v1;
  //void* v2;
  long s1;
  //long s2;
  CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
  wc.flush_callback = flush;
  wc.cleaner_callback = cleaner_callback;
  r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, TRUE, NULL);
  PAIR_ATTR attr = make_pair_attr(8);
  attr.cache_pressure_size = 8;
  r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, attr);

  cleaner_called = FALSE;
  r = toku_cachetable_begin_checkpoint(ct, NULL);
  assert_zero(r);
  toku_cleaner_thread(ct);
  assert(!cleaner_called);
  r = toku_cachetable_end_checkpoint(
      ct, 
      NULL, 
      NULL,
      NULL
      );
  assert(r==0);

  toku_cachetable_verify(ct);
  r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0);
  r = toku_cachetable_close(&ct); lazy_assert_zero(r);


}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test();
  return 0;
}
