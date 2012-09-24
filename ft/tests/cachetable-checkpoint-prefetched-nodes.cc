/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"
#include "cachetable-test.h"

uint64_t clean_val = 0;
uint64_t dirty_val = 0;

bool check_me;
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
  // if the checkpoint is pending, assert that it is of what we made dirty
  if (check_me) {
    flush_called = true;
    assert(c);
    assert(e == &dirty_val);
    assert(v == &dirty_val);
    assert(keep);
    assert(w);
  }
}

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       void** UU(dd),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
  *dirtyp = 0;
  if (extraargs) {
      *value = &dirty_val;
      *dirtyp = true;
  }
  else {
      *value = &clean_val;
      *dirtyp = false;
  }
  *sizep = make_pair_attr(8);
  return 0;
}

static void
cachetable_test (void) {
  const int test_limit = 20;
  int r;
  CACHETABLE ct;
  toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
  char fname1[] = __SRCFILE__ "test1.dat";
  unlink(fname1);
  CACHEFILE f1;
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
  create_dummy_functions(f1);

  bool doing_prefetch = false;
  CACHETABLE_WRITE_CALLBACK wc = def_write_callback(&dirty_val);
  wc.flush_callback = flush;
  r = toku_cachefile_prefetch(f1, make_blocknum(1), 1, wc, fetch, def_pf_req_callback, def_pf_callback, &dirty_val, &doing_prefetch);
  assert(doing_prefetch);
  doing_prefetch = false;
  wc.write_extraargs = NULL;
  r = toku_cachefile_prefetch(f1, make_blocknum(2), 2, wc, fetch, def_pf_req_callback, def_pf_callback, NULL, &doing_prefetch);
  assert(doing_prefetch);

  //
  // Here is the test, we have two pairs, v1 is dirty, v2 is clean, but both are currently pinned
  // Then we will begin a checkpoint, which should theoretically mark both as pending, but
  // flush will be called only for v1, because v1 is dirty
  //
  CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
  toku_cachetable_begin_checkpoint(cp, NULL);


  check_me = true;
  flush_called = false;
  toku_cachetable_end_checkpoint(
      cp, 
      NULL, 
      NULL,
      NULL
      );
  assert(r==0);
  assert(flush_called);
  check_me = false;
  


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
