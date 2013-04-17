/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include "test.h"

CACHEFILE f1;

static int
cleaner_callback(
    void* UU(ftnode_pv),
    BLOCKNUM blocknum,
    uint32_t fullhash,
    void* UU(extraargs)
    )
{
    int r = toku_test_cachetable_unpin(f1,blocknum, fullhash,CACHETABLE_CLEAN,make_pair_attr(8));
    assert(r==0);
    return 0;
}

static void
cachetable_test (void) {
  const int test_limit = 400;
  int r;
  CACHETABLE ct;
  toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
  toku_set_cleaner_period(ct, 1);

  char fname1[] = __SRCFILE__ "test1.dat";
  unlink(fname1);
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

  void* v1;
  //void* v2;
  long s1;
  //long s2;
  for (int j = 0; j < 50000; j++) {
      for (int i = 0; i < 10; i++) {
          CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
          wc.cleaner_callback = cleaner_callback;
          r = toku_cachetable_get_and_pin(f1, make_blocknum(i), i, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
          r = toku_test_cachetable_unpin(f1, make_blocknum(i), i, CACHETABLE_DIRTY, make_pair_attr(8));
      }
      toku_cachefile_flush(f1);
  }
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
