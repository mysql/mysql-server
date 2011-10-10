#ident "$Id: cachetable-simple-verify.c 34757 2011-09-14 19:12:42Z leifwalsh $"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"



BOOL v1_written;
u_int64_t val1;
BOOL v2_written;
u_int64_t val2;
u_int64_t val3;
BOOL check_me;


static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void *e     __attribute__((__unused__)),
       long s      __attribute__((__unused__)),
        long* new_size      __attribute__((__unused__)),
       BOOL w      __attribute__((__unused__)),
       BOOL keep   __attribute__((__unused__)),
       BOOL c      __attribute__((__unused__))
       ) {
    /* Do nothing */
    if (verbose) { printf("FLUSH: %d\n", (int)k.b); }
    //usleep (5*1024*1024);
    if(check_me) {
        assert(c);
        assert(keep);
        assert(w);
        if (v == &val1) {
            v1_written = TRUE;
        }
        else if (v == &val2) {
            v2_written = TRUE;
        }
        else {
            assert(FALSE);
        }
    }
}

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       u_int32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       long *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
  *dirtyp = 0;
  *value = extraargs;
  *sizep = 8;
  return 0;
}

static void 
pe_est_callback(
    void* UU(brtnode_pv), 
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    *bytes_freed_estimate = 0;
    *cost = PE_CHEAP;
}

static int 
pe_callback (
    void *brtnode_pv __attribute__((__unused__)), 
    long bytes_to_free __attribute__((__unused__)), 
    long* bytes_freed, 
    void* extraargs __attribute__((__unused__))
    ) 
{
    *bytes_freed = bytes_to_free;
    return 0;
}

static BOOL pf_req_callback(void* UU(brtnode_pv), void* UU(read_extraargs)) {
  return FALSE;
}

static int pf_callback(void* UU(brtnode_pv), void* UU(read_extraargs), int UU(fd), long* UU(sizep)) {
  assert(FALSE);
}

static void fake_ydb_lock(void) {
}

static void fake_ydb_unlock(void) {
}


static void
cachetable_test (BOOL write_first, BOOL write_second) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    void* v2;
    void* v3;
    long s1;
    long s2;
    long s3;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, flush, fetch, pe_est_callback, pe_callback, pf_req_callback, pf_callback, &val1, NULL);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(2), 2, &v2, &s2, flush, fetch, pe_est_callback, pe_callback, pf_req_callback, pf_callback, &val2, NULL);

    CACHEFILE dependent_cfs[2];
    dependent_cfs[0] = f1;
    dependent_cfs[1] = f1;
    CACHEKEY dependent_keys[2];
    dependent_keys[0] = make_blocknum(1);
    dependent_keys[1] = make_blocknum(2);
    u_int32_t dependent_fullhash[2];
    dependent_fullhash[0] = 1;
    dependent_fullhash[1] = 2;
    // now we set the dirty state of these two.
    enum cachetable_dirty cd[2];
    cd[0] = write_first ? CACHETABLE_DIRTY : CACHETABLE_CLEAN;
    cd[1] = write_second ? CACHETABLE_DIRTY : CACHETABLE_CLEAN;
    //
    // should mark the v1 and v2 as pending
    //
    r = toku_cachetable_begin_checkpoint(ct, NULL); assert(r==0);
    //
    // This call should cause a flush for both
    //
    check_me = TRUE;
    v1_written = FALSE;
    v2_written = FALSE;
    r = toku_cachetable_get_and_pin_with_dep_pairs(
        f1,
        make_blocknum(3),
        3,
        &v3,
        &s3,
        flush, fetch, pe_est_callback, pe_callback, pf_req_callback, pf_callback,
        &val3,
        NULL,
        2, //num_dependent_pairs
        dependent_cfs,
        dependent_keys,
        dependent_fullhash,
        cd
        );
    assert(v1_written == write_first);
    assert(v2_written == write_second);
        
    check_me = FALSE;
    r = toku_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, 8);
    r = toku_cachetable_unpin(f1, make_blocknum(2), 2, CACHETABLE_CLEAN, 8);
    r = toku_cachetable_unpin(f1, make_blocknum(3), 3, CACHETABLE_CLEAN, 8);

    r = toku_cachetable_end_checkpoint(
        ct, 
        NULL, 
        fake_ydb_lock,
        fake_ydb_unlock,
        NULL,
        NULL
        );
    assert(r==0);


    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);


}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test(FALSE,FALSE);
  cachetable_test(FALSE,TRUE);
  cachetable_test(TRUE,FALSE);
  cachetable_test(TRUE,TRUE);
  return 0;
}
