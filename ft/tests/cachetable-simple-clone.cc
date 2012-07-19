/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

BOOL clone_called;
BOOL check_flush;
BOOL flush_expected;
BOOL flush_called;

static void 
clone_callback(void* UU(value_data), void** cloned_value_data, PAIR_ATTR* new_attr, BOOL UU(for_checkpoint), void* UU(write_extraargs))
{
    *cloned_value_data = (void *)1;
    new_attr->is_valid = FALSE;
    clone_called = TRUE;
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
    BOOL w      __attribute__((__unused__)),
    BOOL keep   __attribute__((__unused__)),
    BOOL c      __attribute__((__unused__)),
    BOOL UU(is_clone)
    ) 
{  
    if (w) usleep(5*1024*1024);
    if (w && check_flush) {
        assert(flush_expected);
        if (clone_called) assert(is_clone);
    }
    flush_called = TRUE;
    if (is_clone) assert(!keep);
}

static uint64_t tdelta_usec(struct timeval *tend, struct timeval *tstart) {
    uint64_t t = tend->tv_sec * 1000000 + tend->tv_usec;
    t -= tstart->tv_sec * 1000000 + tstart->tv_usec;
    return t;
}


//
// test the following things for simple cloning:
//  - if the pending pair is clean, nothing gets written
//  - if the pending pair is dirty and cloneable, then pair is written
//     in background and get_and_pin returns immedietely
//  - if the pending pair is dirty and not cloneable, then get_and_pin
//     blocks until the pair is written out
//
static void
test_clean (enum cachetable_dirty dirty, BOOL cloneable) {
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
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, TRUE, NULL);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, dirty, make_pair_attr(8));
    
    check_flush = TRUE;
    clone_called = FALSE;
    flush_expected = (dirty == CACHETABLE_DIRTY) ? TRUE : FALSE;
    flush_called = FALSE;
    // begin checkpoint, since pair is clean, we should not 
    // have the clone called
    r = toku_cachetable_begin_checkpoint(ct, NULL);
    assert_zero(r);
    struct timeval tstart;
    struct timeval tend; 
    gettimeofday(&tstart, NULL);

    // test that having a pin that passes FALSE for may_modify_value does not stall behind checkpoint
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, FALSE, NULL);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    gettimeofday(&tend, NULL);
    assert(tdelta_usec(&tend, &tstart) <= 2000000); 
    assert(!clone_called);
    
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, TRUE, NULL);
    gettimeofday(&tend, NULL);
    
    // we take 5 seconds for a write
    // we check if time to pin is less than 2 seconds, if it is
    // then we know act of cloning worked properly
    if (cloneable || !dirty ) {
        assert(tdelta_usec(&tend, &tstart) <= 2000000); 
    }
    else {
        assert(tdelta_usec(&tend, &tstart) >= 2000000); 
    }

    
    if (dirty == CACHETABLE_DIRTY && cloneable) {
        assert(clone_called);
    }
    else {
        assert(!clone_called);
    }

    // at this point, there should be no more dirty writes
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    gettimeofday(&tend, NULL);
    if (cloneable || !dirty ) {
        assert(tdelta_usec(&tend, &tstart) <= 2000000); 
    }
    else {
        assert(tdelta_usec(&tend, &tstart) >= 2000000); 
    }

    r = toku_cachetable_end_checkpoint(
        ct, 
        NULL, 
        NULL,
        NULL
        );
    assert_zero(r);
    
    check_flush = FALSE;
    
    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  test_clean(CACHETABLE_CLEAN, TRUE);
  test_clean(CACHETABLE_DIRTY, TRUE);
  test_clean(CACHETABLE_CLEAN, FALSE);
  test_clean(CACHETABLE_DIRTY, FALSE);
  return 0;
}
