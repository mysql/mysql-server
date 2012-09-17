/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"
#include "cachetable-test.h"

static void 
clone_callback(void* UU(value_data), void** cloned_value_data, PAIR_ATTR* new_attr, bool UU(for_checkpoint), void* UU(write_extraargs))
{
    *cloned_value_data = (void *)1;
    new_attr->is_valid = false;
}

bool clone_flush_started;
bool clone_flush_completed;
CACHETABLE ct;

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
    bool is_clone,
    bool UU(aggressive)
    ) 
{  
    if (is_clone) {
        clone_flush_started = true;
        usleep(4*1024*1024);
        clone_flush_completed = true;
    }
}

static void *run_end_checkpoint(void *arg) {
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    int r = toku_cachetable_end_checkpoint(
        cp, 
        false, 
        NULL,
        NULL
        );
    assert_zero(r);
    return arg;
}

//
// this test verifies that a PAIR that undergoes a checkpoint on the checkpoint thread is still pinnable while being written out
//
static void
cachetable_test (void) {
    const int test_limit = 200;
    int r;
    ct = NULL;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    create_dummy_functions(f1);

    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;
    wc.clone_callback = clone_callback;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    assert_zero(r);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));
    assert_zero(r);
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_cachetable_begin_checkpoint(cp);


    clone_flush_started = false;
    clone_flush_completed = false;
    toku_pthread_t checkpoint_tid;
    r = toku_pthread_create(&checkpoint_tid, NULL, run_end_checkpoint, NULL); 
    assert_zero(r);    

    usleep(1*1024*1024);

    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    assert_zero(r);
    assert(clone_flush_started && !clone_flush_completed);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert_zero(r);
    
    void *ret;
    r = toku_pthread_join(checkpoint_tid, &ret); 
    assert_zero(r);
    assert(clone_flush_started && clone_flush_completed);

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
