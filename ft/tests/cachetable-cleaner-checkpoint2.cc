/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "test.h"
#include "cachetable-test.h"

CACHEFILE f1;

bool flush_called;

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
  PAIR_ATTR attr = make_pair_attr(8);
  attr.cache_pressure_size = 0;
  *new_size = attr;
  if (w) {
      assert(!flush_called);
      assert(c);
      flush_called = true;
  }
}

bool cleaner_called;

static int
cleaner_callback(
    void* UU(ftnode_pv),
    BLOCKNUM blocknum,
    uint32_t fullhash,
    void* UU(extraargs)
    )
{
    assert(blocknum.b == 1);
    assert(fullhash == 1);
    assert(!cleaner_called);
    assert(flush_called);
    cleaner_called = true;
    int r = toku_test_cachetable_unpin(f1, blocknum, fullhash, CACHETABLE_CLEAN, make_pair_attr(8));
    assert_zero(r);
    return 0;
}


static void
cachetable_test (void) {
  const int test_limit = 12;
  int r;
  CACHETABLE ct;
  toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
  const char *fname1 = TOKU_TEST_FILENAME;
  unlink(fname1);
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
  create_dummy_functions(f1);

  void* v1;
  //void* v2;
  long s1;
  //long s2;
  CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
  wc.flush_callback = flush;
  wc.cleaner_callback = cleaner_callback;
  r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
  PAIR_ATTR attr = make_pair_attr(8);
  attr.cache_pressure_size = 8;
  r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, attr);

  cleaner_called = false;
  CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
  toku_cachetable_begin_checkpoint(cp, NULL);
  assert_zero(r);
  toku_cleaner_thread_for_test(ct);
  assert(!cleaner_called);
  toku_cachetable_end_checkpoint(
      cp, 
      NULL, 
      NULL,
      NULL
      );
  assert(r==0);

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
