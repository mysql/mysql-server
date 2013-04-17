/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include "test.h"
#include "cachetable-test.h"

static void remove_key_expect_checkpoint(
    CACHEKEY* UU(cachekey), 
    bool for_checkpoint, 
    void* UU(extra)
    ) 
{
    assert(for_checkpoint);
}

static void remove_key_expect_no_checkpoint(
    CACHEKEY* UU(cachekey), 
    bool for_checkpoint, 
    void* UU(extra)
    ) 
{
    assert(!for_checkpoint);
}

static void
cachetable_test (void) {
  const int test_limit = 120;
  int r;
  CACHETABLE ct;
  toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
  const char *fname1 = TOKU_TEST_FILENAME;
  unlink(fname1);
  CACHEFILE f1;
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
  create_dummy_functions(f1);

  void* v1;
  //void* v2;
  long s1;
  //long s2;
  CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
  r = toku_cachetable_get_and_pin(f1, make_blocknum(1), toku_cachetable_hash(f1, make_blocknum(1)), &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
  CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
  toku_cachetable_begin_checkpoint(cp, NULL);
  r = toku_test_cachetable_unpin_and_remove(f1, make_blocknum(1), remove_key_expect_checkpoint, NULL);  
  toku_cachetable_end_checkpoint(
      cp, 
      NULL, 
      NULL,
      NULL
      );

  r = toku_cachetable_get_and_pin(f1, make_blocknum(1), toku_cachetable_hash(f1, make_blocknum(1)), &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
  r = toku_test_cachetable_unpin_and_remove(f1, make_blocknum(1), remove_key_expect_no_checkpoint, NULL);  
  
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
