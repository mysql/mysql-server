#ident "$Id: cachetable-simple-verify.c 36689 2011-11-07 22:08:05Z zardosht $"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

static void remove_key_expect_checkpoint(
    CACHEKEY* UU(cachekey), 
    BOOL for_checkpoint, 
    void* UU(extra)
    ) 
{
    assert(for_checkpoint);
}

static void remove_key_expect_no_checkpoint(
    CACHEKEY* UU(cachekey), 
    BOOL for_checkpoint, 
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
  r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
  char fname1[] = __SRCFILE__ "test1.dat";
  unlink(fname1);
  CACHEFILE f1;
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

  void* v1;
  //void* v2;
  long s1;
  //long s2;
  CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
  r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, TRUE, NULL);
  r = toku_cachetable_begin_checkpoint(ct, NULL); assert(r == 0);
  r = toku_cachetable_unpin_and_remove(f1, make_blocknum(1), remove_key_expect_checkpoint, NULL);  
  r = toku_cachetable_end_checkpoint(
      ct, 
      NULL, 
      fake_ydb_lock,
      fake_ydb_unlock,
      NULL,
      NULL
      );
  assert(r==0);

  r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, TRUE, NULL);
  r = toku_cachetable_unpin_and_remove(f1, make_blocknum(1), remove_key_expect_no_checkpoint, NULL);  

  
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
