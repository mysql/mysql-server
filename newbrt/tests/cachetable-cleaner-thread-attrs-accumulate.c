#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

//
// This test verifies that the cleaner thread doesn't call the callback if
// nothing needs flushing.
//

toku_pthread_mutex_t attr_mutex;

// used to access engine status variables 
#define STATUS_VALUE(x) ct_status.status[x].value.num

const PAIR_ATTR attrs[] = {
    { .size = 20, .nonleaf_size = 13, .leaf_size = 900, .rollback_size = 123, .cache_pressure_size = 403, .is_valid = TRUE },
    { .size = 21, .nonleaf_size = 16, .leaf_size = 910, .rollback_size = 113, .cache_pressure_size = 401, .is_valid = TRUE },
    { .size = 22, .nonleaf_size = 17, .leaf_size = 940, .rollback_size = 133, .cache_pressure_size = 402, .is_valid = TRUE },
    { .size = 23, .nonleaf_size = 18, .leaf_size = 931, .rollback_size = 153, .cache_pressure_size = 404, .is_valid = TRUE },
    { .size = 25, .nonleaf_size = 19, .leaf_size = 903, .rollback_size = 173, .cache_pressure_size = 413, .is_valid = TRUE },
    { .size = 26, .nonleaf_size = 10, .leaf_size = 903, .rollback_size = 193, .cache_pressure_size = 423, .is_valid = TRUE },
    { .size = 20, .nonleaf_size = 11, .leaf_size = 902, .rollback_size = 103, .cache_pressure_size = 433, .is_valid = TRUE },
    { .size = 29, .nonleaf_size = 12, .leaf_size = 909, .rollback_size = 113, .cache_pressure_size = 443, .is_valid = TRUE }
};
const int n_pairs = (sizeof attrs) / (sizeof attrs[0]);

static void
flush (CACHEFILE f __attribute__((__unused__)),
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
       ) {
    PAIR_ATTR *expect = e;
    if (!keep) {
        int r = toku_pthread_mutex_lock(&attr_mutex);   // purpose is to make this function single-threaded
        resource_assert_zero(r);
        expect->size -= s.size;
        expect->nonleaf_size -= s.nonleaf_size;
        expect->leaf_size -= s.leaf_size;
        expect->rollback_size -= s.rollback_size;
        expect->cache_pressure_size -= s.cache_pressure_size;
        r = toku_pthread_mutex_unlock(&attr_mutex);
        resource_assert_zero(r);
    }
}

static void
run_test (void) {
    const int test_limit = 1000;
    int r;
    CACHETABLE ct;
    r = toku_pthread_mutex_init(&attr_mutex, NULL); resource_assert_zero(r);
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);

    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    CACHETABLE_STATUS_S ct_status;
    toku_cachetable_get_status(ct, &ct_status);
    assert(STATUS_VALUE(CT_SIZE_NONLEAF) == 0);
    assert(STATUS_VALUE(CT_SIZE_LEAF) == 0);
    assert(STATUS_VALUE(CT_SIZE_ROLLBACK) == 0);
    assert(STATUS_VALUE(CT_SIZE_CACHEPRESSURE) == 0);

    void* vs[n_pairs];
    //void* v2;
    long ss[n_pairs];
    //long s2;
    PAIR_ATTR expect = { .size = 0, .nonleaf_size = 0, .leaf_size = 0, .rollback_size = 0, .cache_pressure_size = 0 };
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;
    wc.write_extraargs = &expect;
    for (int i = 0; i < n_pairs; ++i) {
        r = toku_cachetable_get_and_pin(f1, make_blocknum(i+1), i+1, &vs[i], &ss[i],
                                        wc,
                                        def_fetch,
                                        def_pf_req_callback,
                                        def_pf_callback,
                                        TRUE, 
                                        &expect);
        assert_zero(r);
        r = toku_cachetable_unpin(f1, make_blocknum(i+1), i+1, CACHETABLE_DIRTY, attrs[i]);
        assert_zero(r);
        expect.size += attrs[i].size;
        expect.nonleaf_size += attrs[i].nonleaf_size;
        expect.leaf_size += attrs[i].leaf_size;
        expect.rollback_size += attrs[i].rollback_size;
        expect.cache_pressure_size += attrs[i].cache_pressure_size;
    }

    toku_cachetable_get_status(ct, &ct_status);
    assert(STATUS_VALUE(CT_SIZE_NONLEAF      ) == (uint64_t) expect.nonleaf_size);
    assert(STATUS_VALUE(CT_SIZE_LEAF         ) == (uint64_t) expect.leaf_size);
    assert(STATUS_VALUE(CT_SIZE_ROLLBACK     ) == (uint64_t) expect.rollback_size);
    assert(STATUS_VALUE(CT_SIZE_CACHEPRESSURE) == (uint64_t) expect.cache_pressure_size);

    void *big_v;
    long big_s;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(n_pairs + 1), n_pairs + 1, &big_v, &big_s,
                                    wc,
                                    def_fetch,
                                    def_pf_req_callback,
                                    def_pf_callback,
                                    TRUE, 
                                    &expect);
    toku_cachetable_unpin(f1, make_blocknum(n_pairs + 1), n_pairs + 1, CACHETABLE_CLEAN,
                          make_pair_attr(test_limit - expect.size + 20));

    usleep(2*1024*1024);

    toku_cachetable_get_status(ct, &ct_status);
    assert(STATUS_VALUE(CT_SIZE_NONLEAF      ) == (uint64_t) expect.nonleaf_size);
    assert(STATUS_VALUE(CT_SIZE_LEAF         ) == (uint64_t) expect.leaf_size);
    assert(STATUS_VALUE(CT_SIZE_ROLLBACK     ) == (uint64_t) expect.rollback_size);
    assert(STATUS_VALUE(CT_SIZE_CACHEPRESSURE) == (uint64_t) expect.cache_pressure_size);

    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0);
    r = toku_cachetable_close(&ct); lazy_assert_zero(r);
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  run_test();
  return 0;
}

#undef STATUS_VALUE
