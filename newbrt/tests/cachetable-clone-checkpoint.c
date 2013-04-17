#ident "$Id: cachetable-simple-verify.c 39504 2012-02-03 16:19:33Z zardosht $"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"


static void 
clone_callback(void* UU(value_data), void** cloned_value_data, PAIR_ATTR* new_attr, BOOL UU(for_checkpoint), void* UU(write_extraargs))
{
    *cloned_value_data = (void *)1;
    new_attr->is_valid = FALSE;
}

BOOL clone_flush_started;
BOOL clone_flush_completed;
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
    BOOL w      __attribute__((__unused__)),
    BOOL keep   __attribute__((__unused__)),
    BOOL c      __attribute__((__unused__)),
    BOOL is_clone
    ) 
{  
    if (is_clone) {
        clone_flush_started = TRUE;
        usleep(4*1024*1024);
        clone_flush_completed = TRUE;
    }
}

static void *run_end_checkpoint(void *arg) {
    int r = toku_cachetable_end_checkpoint(
        ct, 
        NULL, 
        fake_ydb_lock,
        fake_ydb_unlock,
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
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;
    wc.clone_callback = clone_callback;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, TRUE, NULL);
    assert_zero(r);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));
    assert_zero(r);
    r = toku_cachetable_begin_checkpoint(ct, NULL);


    clone_flush_started = FALSE;
    clone_flush_completed = FALSE;
    toku_pthread_t checkpoint_tid;
    r = toku_pthread_create(&checkpoint_tid, NULL, run_end_checkpoint, NULL); 
    assert_zero(r);    

    usleep(1*1024*1024);

    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, TRUE, NULL);
    assert_zero(r);
    assert(clone_flush_started && !clone_flush_completed);
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert_zero(r);
    
    void *ret;
    r = toku_pthread_join(checkpoint_tid, &ret); 
    assert_zero(r);
    assert(clone_flush_started && clone_flush_completed);

    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test();
  return 0;
}
