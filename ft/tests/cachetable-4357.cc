/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "test.h"

CACHEFILE f1;

static void *pin_nonblocking(void *arg) {    
    void* v1;
    long s1;
    int r = toku_cachetable_get_and_pin_nonblocking(
        f1, 
        make_blocknum(1), 
        toku_cachetable_hash(f1, make_blocknum(1)), 
        &v1, 
        &s1, 
        def_write_callback(NULL), def_fetch, def_pf_req_callback, def_pf_callback, 
        PL_WRITE_EXPENSIVE,
        NULL, 
        NULL
        );
    assert(r==TOKUDB_TRY_AGAIN);
    return arg;
}


static void
cachetable_test (void) {
  const int test_limit = 12;
  int r;
  CACHETABLE ct;
  toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
  char fname1[] = __SRCFILE__ "test1.dat";
  unlink(fname1);
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

  void* v1;
  long s1;
  r = toku_cachetable_get_and_pin(
      f1, 
      make_blocknum(1), 
      toku_cachetable_hash(f1, make_blocknum(1)), 
      &v1, 
      &s1, 
      def_write_callback(NULL), def_fetch, def_pf_req_callback, def_pf_callback, 
      true, 
      NULL
      );
  toku_pthread_t pin_nonblocking_tid;
  r = toku_pthread_create(&pin_nonblocking_tid, NULL, pin_nonblocking, NULL); 
  assert_zero(r);    
  // sleep 3 seconds
  usleep(3*1024*1024);
  r = toku_test_cachetable_unpin_and_remove(f1, make_blocknum(1), NULL, NULL);
  assert_zero(r);
  
  void *ret;
  r = toku_pthread_join(pin_nonblocking_tid, &ret); 
  assert_zero(r);
  
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
