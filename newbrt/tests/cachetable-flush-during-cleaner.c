#ident "$Id: cachetable-simple-verify.c 36579 2011-11-04 20:02:04Z zardosht $"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

CACHEFILE f1;

static int
cleaner_callback(
    void* UU(brtnode_pv),
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    void* UU(extraargs)
    )
{
    int r = toku_cachetable_unpin(f1,blocknum, fullhash,CACHETABLE_CLEAN,make_pair_attr(8));
    assert(r==0);
    return 0;
}



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
  const int test_limit = 400;
  int r;
  CACHETABLE ct;
  r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
  r = toku_set_cleaner_period(ct, 1); assert(r == 0);

  char fname1[] = __FILE__ "test1.dat";
  unlink(fname1);
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

  void* v1;
  //void* v2;
  long s1;
  //long s2;
  for (int j = 0; j < 50000; j++) {
      for (int i = 0; i < 10; i++) {
          r = toku_cachetable_get_and_pin(f1, make_blocknum(i), i, &v1, &s1, flush, fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, cleaner_callback, NULL, NULL);
          r = toku_cachetable_unpin(f1, make_blocknum(i), i, CACHETABLE_DIRTY, make_pair_attr(8));
      }
      r = toku_cachefile_flush(f1);
      assert(r == 0);
  }
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
