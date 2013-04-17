#ident "$Id: cachetable-simple-verify.c 36689 2011-11-07 22:08:05Z zardosht $"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

CACHEFILE f1;


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
  PAIR_ATTR attr = make_pair_attr(8);
  attr.cache_pressure_size = 8;
  *new_size = attr;
  if (w) {
      assert(c);
  }
}

static int
cleaner_callback(
    void* UU(brtnode_pv),
    BLOCKNUM UU(blocknum),
    u_int32_t UU(fullhash),
    void* UU(extraargs)
    )
{
    assert(FALSE);
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
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

  void* v1;
  //void* v2;
  long s1;
  //long s2;
  CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
  wc.flush_callback = flush;
  wc.cleaner_callback = cleaner_callback;
  r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, NULL);
  PAIR_ATTR attr = make_pair_attr(8);
  attr.cache_pressure_size = 8;
  r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, attr);

  // test that once we have redirected to /dev/null,
  // cleaner callback is NOT called
  r = toku_cachefile_redirect_nullfd(f1);
  assert_zero(r);

  toku_cleaner_thread(ct);

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
