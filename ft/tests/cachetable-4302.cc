/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

CACHETABLE ct;
bool checkpoint_began;

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
}

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value,
       void** UU(dd),
       PAIR_ATTR *sizep,
       int  *dirtyp,
       void *extraargs
       ) {
  *dirtyp = 0;
  *value = extraargs;
  *sizep = make_pair_attr(8);
  return 0;
}

static void 
pe_est_callback(
    void* UU(ftnode_pv), 
    void* UU(dd),
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    *bytes_freed_estimate = 1;
    *cost = PE_CHEAP;
}

static int 
pe_callback (
    void *ftnode_pv __attribute__((__unused__)), 
    PAIR_ATTR bytes_to_free, 
    PAIR_ATTR* bytes_freed, 
    void* extraargs __attribute__((__unused__))
    ) 
{
    //
    // The purpose of this test is to verify the fix for #4302.
    // The problem with #4302 was as follows. During 
    // toku_cachetable_put_with_dep_pairs, there is a region
    // where we assert that no checkpoint begins. In that region,
    // we were calling maybe_flush_some, which releases the
    // cachetable lock and calls pe_callback here. Beginning a 
    // checkpoint in this time frame causes an assert to fail.
    // So, before the fix for #4302, an assert would fail when calling
    // begin_checkpoint here. If at some point in the future, this call here
    // causes a deadlock, then we need to find another way to ensure that
    // a checkpoint that begins during an eviction caused by 
    // toku_cachetable_put_with_dep_pairs does not cause a crash.
    //
    if (!checkpoint_began) {
        int r = toku_cachetable_begin_checkpoint(ct, NULL); assert(r == 0);
        checkpoint_began = true;
    }
    *bytes_freed = make_pair_attr(bytes_to_free.size - 1);
    return 0;
}


static void
test_get_key_and_fullhash(
    CACHEKEY* cachekey,
    uint32_t* fullhash,
    void* UU(extra))
{
    CACHEKEY name;
    name.b = 2;
    *cachekey = name;
    *fullhash = 2;
}

static void
cachetable_test (void) {
  const int test_limit = 12;
  int r;
  r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
  char fname1[] = __SRCFILE__ "test1.dat";
  unlink(fname1);
  CACHEFILE f1;
  r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

  void* v1;
  long s1;
  uint64_t val1 = 0;
  uint64_t val2 = 0;
  CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
  wc.flush_callback = flush;
  wc.pe_est_callback = pe_est_callback;
  wc.pe_callback = pe_callback;
  r = toku_cachetable_get_and_pin(
      f1, 
      make_blocknum(1), 
      1, 
      &v1, 
      &s1, 
      wc, 
      fetch, 
      def_pf_req_callback, def_pf_callback, 
      true, 
      &val1
      );
  r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
  CACHEKEY key;
  uint32_t fullhash;
  checkpoint_began = false;
  r = toku_cachetable_put_with_dep_pairs(
        f1,
        test_get_key_and_fullhash,
        &val2,
        make_pair_attr(8),
        wc,
        NULL,
        0, // number of dependent pairs that we may need to checkpoint
        NULL, // array of cachefiles of dependent pairs
        NULL, // array of cachekeys of dependent pairs
        NULL, //array of fullhashes of dependent pairs
        NULL, // array stating dirty/cleanness of dependent pairs
        &key,
        &fullhash
        );
  r = toku_cachetable_unpin(f1, make_blocknum(2), 2, CACHETABLE_CLEAN, make_pair_attr(8));

  // end the checkpoint began in pe_callback
  assert(checkpoint_began);
  r = toku_cachetable_end_checkpoint(
      ct, 
      NULL, 
      NULL,
      NULL
      );

  toku_cachetable_verify(ct);
  r = toku_cachefile_close(&f1, 0, false, ZERO_LSN); assert(r == 0);
  r = toku_cachetable_close(&ct); lazy_assert_zero(r);


}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test();
  return 0;
}
