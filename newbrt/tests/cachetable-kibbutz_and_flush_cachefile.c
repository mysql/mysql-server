#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

BOOL foo;

//
// This test verifies that flushing a cachefile will wait on kibbutzes to finish
//
static void kibbutz_work(void *fe_v)
{
    CACHEFILE f1 = fe_v;
    sleep(2);
    foo = TRUE;
    int r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r==0);
    remove_background_job(f1, false);    
}


static void
run_test (void) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    //void* v2;
    long s1;
    //long s2;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, def_flush, def_fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, NULL, NULL);
    foo = FALSE;
    cachefile_kibbutz_enq(f1, kibbutz_work, f1);
    r = toku_cachefile_flush(f1); assert(r == 0);
    assert(foo);
    assert(f1);
    
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, def_flush, def_fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, NULL, NULL);
    foo = FALSE;
    cachefile_kibbutz_enq(f1, kibbutz_work, f1);
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    assert(foo);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);
    
    
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  run_test();
  return 0;
}
