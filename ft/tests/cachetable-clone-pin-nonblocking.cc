/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

static void 
clone_callback(void* UU(value_data), void** cloned_value_data, PAIR_ATTR* new_attr, bool UU(for_checkpoint), void* UU(write_extraargs))
{
    *cloned_value_data = (void *)1;
    new_attr->is_valid = false;
}

static void
flush (
    CACHEFILE f __attribute__((__unused__)),
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
    ) 
{  
}


// this test verifies that a partial fetch will wait for a cloned pair to complete
// writing to disk
static void
cachetable_test (enum cachetable_dirty dirty, bool cloneable) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.clone_callback = cloneable ? clone_callback : NULL;
    wc.flush_callback = flush;
    
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, dirty, make_pair_attr(8));

    // test that having a pin that passes false for may_modify_value does not stall behind checkpoint
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_cachetable_begin_checkpoint(cp, NULL); assert_zero(r);
    r = toku_cachetable_get_and_pin_nonblocking(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, PL_READ, NULL, NULL);
    assert(r == 0);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r == 0);

    r = toku_cachetable_get_and_pin_nonblocking(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, PL_WRITE_EXPENSIVE, NULL, NULL);
    if (dirty == CACHETABLE_DIRTY && !cloneable) {
        assert(r == TOKUDB_TRY_AGAIN);
    }
    else {
        assert(r == 0);
        r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    }

    r = toku_cachetable_end_checkpoint(
        cp, 
        NULL, 
        NULL,
        NULL
        );
    assert_zero(r);


    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, 0, false, ZERO_LSN); assert(r == 0);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);


}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test(CACHETABLE_DIRTY, true);
  cachetable_test(CACHETABLE_DIRTY, false);
  cachetable_test(CACHETABLE_CLEAN, true);
  cachetable_test(CACHETABLE_CLEAN, false);
  return 0;
}
