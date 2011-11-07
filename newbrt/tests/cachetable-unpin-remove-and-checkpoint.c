#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

CACHETABLE ct;

//
// This test exposed a bug (#3970) caught only by Valgrind.
// freed memory was being accessed by toku_cachetable_unpin_and_remove
//
static void *run_end_chkpt(void *arg) {
    assert(arg == NULL);
    int r = toku_cachetable_end_checkpoint(
        ct, 
        NULL, 
        fake_ydb_lock,
        fake_ydb_unlock,
        NULL,
        NULL
        );
    assert(r==0);
    return arg;
}

static void
run_test (void) {
    const int test_limit = 12;
    int r;
    ct = NULL;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    //void* v2;
    long s1;
    //long s2;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), toku_cachetable_hash(f1, make_blocknum(1)), &v1, &s1, def_flush, def_fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, NULL, NULL);
    toku_cachetable_unpin(
        f1, 
        make_blocknum(1), 
        toku_cachetable_hash(f1, make_blocknum(1)),
        CACHETABLE_DIRTY,
        make_pair_attr(8)
        );

    // now this should mark the pair for checkpoint
    r = toku_cachetable_begin_checkpoint(ct, NULL);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), toku_cachetable_hash(f1, make_blocknum(1)), &v1, &s1, def_flush, def_fetch, def_pe_est_callback, def_pe_callback, def_pf_req_callback, def_pf_callback, def_cleaner_callback, NULL, NULL);

    toku_pthread_t mytid;
    r = toku_pthread_create(&mytid, NULL, run_end_chkpt, NULL);
    assert(r==0);

    // give checkpoint thread a chance to start waiting on lock
    sleep(1);
    r = toku_cachetable_unpin_and_remove(f1, make_blocknum(1), FALSE);
    assert(r==0);

    void* ret;
    r = toku_pthread_join(mytid, &ret);
    assert(r==0);
    
    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);
    
    
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  run_test();
  return 0;
}
